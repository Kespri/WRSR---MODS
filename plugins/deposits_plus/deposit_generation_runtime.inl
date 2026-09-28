// Engine adapter. All texture access runs once at the start of Terrain::Render;
// saving reads the DDS files the native save just wrote (never maps a GPU texture
// from the save/simulation thread). The native maps remain authoritative.
#include "deposit_country.h"
namespace {
struct GenerationLock {
    GenerationLock() { EnterCriticalSection(&g_lock); }
    ~GenerationLock() { LeaveCriticalSection(&g_lock); }
};
struct GenerationMap { int id; unsigned w=0,h=0; std::vector<DWORD> pixels; };
static DG::State g_generationState;
static std::string g_generationFolder;
static bool g_generationPending=false,g_generationReady=false,g_generationFault=false;
static bool g_generationHadState=false,g_generationLegacy=false;
static bool g_desertMap=false;   // 0.4.1: the world's script.ini carries $TYPE_DESERT
static unsigned g_generationWait=0;
static void* g_generationTerrain=NULL;
static void* g_generationWater=NULL;
typedef void (*GenRender)(void*,bool,void*,void*,int,int);
typedef int (*GenTerrainInit)(void*,void*,char*,void*,void*,void*,const char*,unsigned,int);
typedef float* (*GenVec)(void*,float*);
typedef float (*GenPointHeight)(void*,const float*);
typedef float (*GenFloat)(void*);
typedef int (*GenInt)(void*);
typedef bool (*GenBool)(void*);
static GenRender o_GenerationRender;
static GenTerrainInit o_GenerationTerrainInit;
static GenVec genOffset,genSize;
static GenPointHeight genHeight;
static GenFloat genWaterHeight,genWaterAmplitude;
static GenBool genNoWater;
static GenInt genHeightWidth,genHeightHeight;
static GenFloat genBoundaryXMin,genBoundaryZMin,genBoundaryXMax,genBoundaryZMax;
static bool genBorderLayoutReady=false;
// WRSR 1.1.1.9: C3D_TERRAIN owns vector<Vec3> at +8A0. Both the engine's
// $BORDER_POLYGON parser and the game's polygon cache use this exact array.
// Guard the layout via the exported constructor's three vector initializers.
static bool GenerationBorderSignature(const BYTE* ctor) {
    static const BYTE init[]={0x48,0x89,0x83,0xa0,0x08,0x00,0x00,
                              0x48,0x89,0x83,0xa8,0x08,0x00,0x00,
                              0x48,0x89,0x83,0xb0,0x08,0x00,0x00};
    return ReadablePtr(ctor,sizeof init+0x17) && !memcmp(ctor+0x17,init,sizeof init);
}
static bool GenerationBorderRead(const void* from,void* to,size_t bytes) {
    __try { if(!ReadablePtr(from,bytes)) return false; memcpy(to,from,bytes); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool GenerationCountry(void* terrain,float sx,float sz,const float* off,DG::Bytes& outside) {
    if(!genBorderLayoutReady) {
        Logf("generation WARN country border layout not verified; no first placement (existing deposits unchanged)"); return false;
    }
    uintptr_t header[3]={},check[3]={};
    if(!GenerationBorderRead((BYTE*)terrain+0x8a0,header,sizeof header) || header[1]<header[0] ||
       header[2]<header[1] || (header[1]-header[0])%sizeof(DG::BorderVertex) ||
       header[1]-header[0]>4096*sizeof(DG::BorderVertex)) {
        Logf("generation WARN country border vector unreadable/invalid; no first placement"); return false;
    }
    std::vector<DG::BorderVertex> vertices((header[1]-header[0])/sizeof(DG::BorderVertex));
    bool polygon=!vertices.empty();
    if(polygon) {
        if(!GenerationBorderRead((const void*)header[0],vertices.data(),vertices.size()*sizeof(DG::BorderVertex)) ||
           !GenerationBorderRead((BYTE*)terrain+0x8a0,check,sizeof check) || memcmp(header,check,sizeof header)) {
            Logf("generation WARN country border changed/unreadable while taking snapshot; retry before placement"); return false;
        }
    } else {
        // No polygon: native rectangular building boundaries are offsets from
        // the terrain's lower/upper edges, NOT the full texture rectangle.
        if(!genBoundaryXMin||!genBoundaryXMax||!genBoundaryZMin||!genBoundaryZMax) return false;
        float x0=off[0]+genBoundaryXMin(terrain),x1=off[0]+sx+genBoundaryXMax(terrain);
        float z0=off[2]+genBoundaryZMin(terrain),z1=off[2]+sz+genBoundaryZMax(terrain);
        if(!(x0<x1 && z0<z1)) { Logf("generation WARN invalid native rectangular country bounds; no first placement"); return false; }
        vertices={{x0,0,z0},{x1,0,z0},{x1,0,z1},{x0,0,z1}};
    }
    std::string error;
    if(!DG::ValidateBorder(vertices,error)) {
        Logf("generation WARN country border rejected: %s; no first placement",error.c_str()); return false;
    }
    outside=DG::CountryMask(vertices,off[0],off[2],sx,sz);
    Logf("generation country: source=%s vertices=%u; entire resource cells stay inside boundary",
         polygon?"native BORDER_POLYGON":"native rectangular bounds",(unsigned)vertices.size());
    return outside.size()==DG::Cells;
}

static std::string GenerationToken(const char* raw) {
    std::string key(raw);
    for(char& c:key) if(c>='a' && c<='z') c=(char)(c-'a'+'A');
    return key;
}
static std::string GenerationResolveFolder(const char* folder) {
    if(!folder || !*folder || strlen(folder)>=MAX_PATH-32) return {};
    char exe[MAX_PATH]={},full[MAX_PATH]={};
    GetModuleFileNameA(NULL,exe,MAX_PATH);
    char* slash=strrchr(exe,'\\'); if(!slash) return {}; *slash=0;
    std::string paths[]={folder,std::string(exe)+"\\"+folder,std::string(exe)+"\\media_soviet\\"+folder};
    for(const auto& p:paths) {
        DWORD n=GetFullPathNameA(p.c_str(),MAX_PATH,full,NULL);
        if(!n || n>=MAX_PATH-32) continue;
        DWORD a=GetFileAttributesA(full);
        if(a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY)) return full;
    }
    return {};
}
static bool GenerationIsSave(const std::string& folder) {
    std::string p=folder; for(char& c:p) { if(c=='/') c='\\'; if(c>='A'&&c<='Z') c+=32; }
    return p.find("\\save\\")!=std::string::npos ||
        p.find("\\saved_last\\")!=std::string::npos ||
        (p.size()>=11 && p.compare(p.size()-11,11,"\\saved_last")==0);
}
static bool GenerationReadFile(const std::string& path,DG::Bytes& out,DWORD maximum=256u*1024*1024) {
    HANDLE f=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                         NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER n={}; DWORD got=0;
    bool ok=GetFileSizeEx(f,&n) && n.QuadPart>=0 && n.QuadPart<=maximum;
    if(ok) { out.resize((size_t)n.QuadPart); ok=ReadFile(f,out.data(),(DWORD)out.size(),&got,NULL) && got==out.size(); }
    CloseHandle(f); return ok;
}
static bool GenerationAtomicWrite(const std::string& folder,const DG::Bytes& data) {
    char temp[MAX_PATH]={};
    if(folder.size()>MAX_PATH-32 || !GetTempFileNameA(folder.c_str(),"tdg",0,temp)) return false;
    HANDLE f=CreateFileA(temp,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    DWORD written=0;
    bool ok=f!=INVALID_HANDLE_VALUE && WriteFile(f,data.data(),(DWORD)data.size(),&written,NULL) &&
            written==data.size() && FlushFileBuffers(f);
    if(f!=INVALID_HANDLE_VALUE) CloseHandle(f);
    if(ok) ok=MoveFileExA(temp,(folder+"\\tesmio_deposits.bin").c_str(),
                         MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) {
        DWORD err=GetLastError();   // before DeleteFileA replaces it
        Logf("generation WARN sidecar write failed in %s (%lu)",folder.c_str(),err);
        DeleteFileA(temp); // only our exact GetTempFileName-created temporary file
    }
    return ok;
}
static DWORD GenerationU32(const DG::Bytes& b,size_t at) {
    return DWORD(b[at])|(DWORD(b[at+1])<<8)|(DWORD(b[at+2])<<16)|(DWORD(b[at+3])<<24);
}
static int GenerationShift(unsigned c) { return c==3?24:16-int(c)*8; }
static std::string GenerationMapPath(const std::string& folder,int map) {
    if(map==64) return folder+"\\mask.dds";
    if(map==0) return folder+"\\resourcemap.dds";
    return folder+"\\resourcemap"+std::to_string(map+1)+".dds";
}
static bool GenerationDds(const std::string& folder,int map,GenerationMap& out) {
    DG::Bytes b;
    if(!GenerationReadFile(GenerationMapPath(folder,map),b,128u*1024*1024) || b.size()<128 ||
       GenerationU32(b,0)!=0x20534444 || GenerationU32(b,4)!=124 || GenerationU32(b,76)!=32 ||
       (GenerationU32(b,80)&4) || GenerationU32(b,88)!=32) return false;
    unsigned w=GenerationU32(b,16),h=GenerationU32(b,12);
    unsigned pitch=GenerationU32(b,20);
    if(!(GenerationU32(b,8)&8)) pitch=w*4;
    if(!w||!h||w>4096||h>4096||pitch<w*4 || pitch>w*4+4096 ||
       128ull+(uint64_t)pitch*h>b.size()) return false;
    unsigned shifts[4];
    for(unsigned c=0;c<4;++c) {
        DWORD mask=GenerationU32(b,92+4*c); unsigned shift=0;
        if(!mask) return false;
        while(!(mask&1)) { mask>>=1; ++shift; }
        if(mask!=255 || shift>24) return false;
        shifts[c]=shift;
        for(unsigned j=0;j<c;++j) if(shifts[j]==shift) return false;
    }
    out.id=map; out.w=w; out.h=h; out.pixels.resize((size_t)w*h);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        DWORD v=GenerationU32(b,128+(size_t)y*pitch+x*4),argb=0;
        for(unsigned c=0;c<4;++c) argb|=((v>>shifts[c])&255)<<GenerationShift(c);
        out.pixels[(size_t)y*w+x]=argb;
    }
    return true;
}
static DG::Bytes GenerationChannel(const GenerationMap& m,int component) {
    DG::Bytes b(m.pixels.size()); int shift=GenerationShift(component);
    for(size_t i=0;i<b.size();++i) b[i]=(unsigned char)(m.pixels[i]>>shift);
    return b;
}
static GenerationMap* GenerationFindMap(std::vector<GenerationMap>& maps,int id) {
    for(auto& m:maps) if(m.id==id) return &m;
    return NULL;
}
static void* GenerationTexture(int map) {
    DepositDef d={}; d.map=map; return DepositMapTexture(&d);
}
// Keep SEH outside functions with C++ destructors. Always close a successfully
// opened access bracket, including when a texture read/write faults.
static bool GenerationTransfer(void* tex,DWORD* pixels,unsigned w,unsigned h,bool write) {
    bool opened=false,ok=false;
    typedef void (*VoidFn)(void*);
    typedef DWORD (*GetFn)(void*,int,int);
    typedef void (*SetFn)(void*,int,int,DWORD);
    VoidFn open=(VoidFn)TexVCall(tex,16),close=(VoidFn)TexVCall(tex,18);
    GetFn get=(GetFn)TexVCall(tex,20); SetFn set=(SetFn)TexVCall(tex,23);
    if(!open||!close||!get||!set) return false;
    __try {
        __try {
            open(tex); opened=true;
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                if(write) set(tex,x,y,pixels[(size_t)y*w+x]);
                else pixels[(size_t)y*w+x]=get(tex,x,y);
            }
            ok=true;
        } __finally { if(opened) close(tex); }
    } __except(FaultFilter("deposit generation texture",GetExceptionInformation())) { ok=false; }
    return ok;
}
static bool GenerationReadMap(int map,GenerationMap& out) {
    void* tex=GenerationTexture(map);
    if(!ReadablePtr(tex,0x1c)) return false;
    int w=*(int*)((BYTE*)tex+0x14),h=*(int*)((BYTE*)tex+0x18);
    if(w<1||h<1||w>4096||h>4096) return false;
    out.id=map; out.w=w; out.h=h; out.pixels.resize((size_t)w*h);
    return GenerationTransfer(tex,out.pixels.data(),w,h,false);
}
static bool GenerationSharedVanilla(const DepositDef& d) {
    return (d.map==0 && d.component<3) || (d.map==1 && d.component<3) || d.map==64;
}
static bool GenerationLand(void* terrain,float sx,float sz,const float* off,DG::Bytes& blocked) {
    bool noWater=genNoWater(terrain);
    float level=0;
    if(!noWater) {
        if(terrain!=g_generationTerrain || !ReadablePtr(g_generationWater,8)) return false;
        level=genWaterHeight(g_generationWater)+fabsf(genWaterAmplitude(g_generationWater))+g_generationClearance;
        if(!std::isfinite(level)) return false;
    }
    unsigned hw=genHeightWidth(terrain),hh=genHeightHeight(terrain);
    if(!hw||!hh||hw>4096||hh>4096||hw%DG::Side||hh%DG::Side) return false;
    unsigned nx=(hw+DG::Side-1)/DG::Side,ny=(hh+DG::Side-1)/DG::Side;
    DG::Bytes outside;
    if(!GenerationCountry(terrain,sx,sz,off,outside)) return false;
    blocked.assign(DG::Cells,0);
    DG::Bytes water(DG::Cells,0);
    unsigned wet=0,country=0,shore=0,freeLand=0;
    for(unsigned y=0;y<DG::Side;++y) for(unsigned x=0;x<DG::Side;++x) {
        const unsigned index=y*DG::Side+x;
        if(outside[index]) { blocked[index]|=DG::Country; ++country; }
        if(x==0||y==0||x==DG::Side-1||y==DG::Side-1) blocked[index]|=DG::MapEdge;
        bool bad=false;
        // All terrain-grid vertices in a resource cell, not just its centre.
        // Bilinear terrain cannot dip below all four of its cell corners.
        for(unsigned iy=0;iy<=ny && !bad;++iy) for(unsigned ix=0;ix<=nx;++ix) {
            __declspec(align(16)) float p[4]={off[0]+(x+float(ix)/nx)*sx/DG::Side,0,
                                           off[2]+(y+float(iy)/ny)*sz/DG::Side,0};
            float height=genHeight(terrain,p);
            if(!std::isfinite(height) || (!noWater && height<=level)) { bad=true; break; }
        }
        water[index]=bad; wet+=bad;
    }
    auto shoreMask=DG::Dilate(water,(unsigned)ceilf(g_generationShore*DG::Side/sx),
                                    (unsigned)ceilf(g_generationShore*DG::Side/sz));
    for(unsigned p=0;p<DG::Cells;++p) {
        if(shoreMask[p]) { blocked[p]|=DG::Water; shore+=!water[p]; }
        freeLand+=blocked[p]==0;
    }
    Logf("generation land mask: country-excluded=%u water=%u shore-extra=%u eligible=%u/%u; water=%s level=%.2f shore=%.1fm (exclusions may overlap)",
         country,wet,shore,freeLand,DG::Cells,noWater?"disabled":"enabled",level,g_generationShore);
    return true;
}
// 0.4.6: terrain height at the centre of every resource cell, for the desert fill's relief.
static bool GenerationHeights(void* terrain,float sx,float sz,const float* off,std::vector<float>& out) {
    out.assign(DG::Cells,0.f);
    for(unsigned y=0;y<DG::Side;++y) for(unsigned x=0;x<DG::Side;++x) {
        __declspec(align(16)) float p[4]={off[0]+(x+0.5f)*sx/DG::Side,0,off[2]+(y+0.5f)*sz/DG::Side,0};
        float h=genHeight(terrain,p); if(!std::isfinite(h)) return false;
        out[y*DG::Side+x]=h;
    }
    return true;
}
static void GenerationMergeBlocked(DG::Bytes& into,const DG::Bytes& pixels,unsigned w,unsigned h) {
    DG::Bytes mask=DG::Occupancy(pixels,w,h);
    for(size_t i=0;i<into.size();++i) if(mask[i]) into[i]=1;
}

static bool GenerationRun(void* terrain) {
    float offset[4]={},size[4]={};
    if(!genOffset(terrain,offset)||!genSize(terrain,size)||!std::isfinite(size[0])||
       !std::isfinite(size[1])||!std::isfinite(offset[0])||!std::isfinite(offset[2])||size[0]<=0||size[1]<=0) return false;
    std::vector<GenerationMap> live;
    // Occupancy must include vanilla oil/iron/coal/uranium/bauxite and gravel.
    for(int id:{0,1,64}) { GenerationMap m; if(!GenerationReadMap(id,m)) return false; live.push_back(std::move(m)); }
    for(int i=0;i<g_depCount;++i) if(!GenerationFindMap(live,g_dep[i].map)) {
        GenerationMap m; if(!GenerationReadMap(g_dep[i].map,m)) return false; live.push_back(std::move(m));
    }
    DG::State candidate=g_generationState;
    std::vector<int> recordIndex(g_depCount,-1),generate;
    unsigned resumed=0;
    std::vector<GenerationMap> disk;
    DG::Bytes desertLand;   // 0.4.1: land mask for desert_fill, computed once when needed
    // Before remapping, validate the native contents against the SAVED layout.
    // A native save that succeeded while the sidecar replacement failed can
    // contain a different channel order. Never guess or restore stale snapshots.
    for(auto& r:candidate.records) if(r.map>=0) {
        GenerationMap* m=GenerationFindMap(disk,r.map);
        if(!m) { GenerationMap d; if(!GenerationDds(g_generationFolder,r.map,d)) {
            Logf("generation WARN cannot read saved source map %d for %s; no remap/generation/save-state overwrite",r.map,r.token.c_str());
            g_generationFault=true; return false;
        } disk.push_back(std::move(d)); m=&disk.back(); }
        DG::Bytes native=GenerationChannel(*m,r.component);
        if(r.width!=m->w || r.height!=m->h || r.pixels!=native) {
            Logf("generation WARN native DDS and metadata disagree for %s (incomplete save, external edit or mismatched backup); no remap/regeneration/overwrite",r.token.c_str());
            g_generationFault=true; return false;
        }
    }
    for(int i=0;i<g_depCount;++i) {
        const DepositDef& d=g_dep[i]; std::string token=GenerationToken(d.token);
        int index=DG::Find(candidate,token);
        GenerationMap* target=GenerationFindMap(live,d.map);
        bool newlyAdded=index<0;
        if(newlyAdded) {
            for(const auto& old:candidate.records) if(old.type==d.type && old.token!=token) {
                Logf("generation WARN [%s] type %d belongs to saved identity %s; refusing reassignment",d.name,d.type,old.token.c_str());
                g_generationFault=true; return false;
            }
            if(candidate.records.size()>=DG::MaxRecords) {
                Logf("generation WARN [%s] saved resource history full; no changes",d.name); g_generationFault=true; return false;
            }
            DG::Record r; r.token=token; r.type=d.type; r.map=d.map; r.component=d.component;
            r.width=target->w; r.height=target->h; r.pixels.assign((size_t)r.width*r.height,0);
            bool occupiedOldSlot=false;
            for(const auto& old:candidate.records) if(old.map==d.map && old.component==d.component) occupiedOldSlot=true;
            if(!g_generationHadState || (!occupiedOldSlot && d.map<2)) r.pixels=GenerationChannel(*target,d.component);
            // Explicit legacy terrain conversion only, never paint the ground.
            // On a world WITH metadata a newly added sand token has no legacy
            // terrain history. Do not adopt decorative ground instead of generating it.
            if(d.legacyTerrainComponent>=0 && g_generationLegacy && !g_generationHadState && !DG::HasData(r.pixels)) {
                auto* mask=GenerationFindMap(live,64);
                DG::Bytes source=GenerationChannel(*mask,d.legacyTerrainComponent);
                for(unsigned y=0;y<r.height;++y) for(unsigned x=0;x<r.width;++x) {
                    uint64_t sum=0; unsigned n=0;
                    unsigned x0=x*mask->w/r.width,x1=(x+1)*mask->w/r.width;
                    unsigned y0=y*mask->h/r.height,y1=(y+1)*mask->h/r.height;
                    for(unsigned yy=y0;yy<(std::max)(y0+1,y1);++yy)
                        for(unsigned xx=x0;xx<(std::max)(x0+1,x1);++xx) { sum+=source[yy*mask->w+xx]; ++n; }
                    r.pixels[(size_t)y*r.width+x]=(unsigned char)(sum/n);
                }
                Logf("generation [%s] adopted old terrain richness into independent storage; terrain untouched",d.name);
            } else if(d.legacyTerrainComponent>=0 && !g_generationHadState && !g_generationLegacy) {
                r.pixels.assign((size_t)r.width*r.height,0);
            }
            candidate.records.push_back(std::move(r)); index=(int)candidate.records.size()-1;
        }
        auto& r=candidate.records[index]; recordIndex[i]=index;
        if(r.type!=d.type || r.width!=target->w || r.height!=target->h) {
            Logf("generation WARN [%s] identity/type or map dimensions changed; refusing unsafe remap",d.name); g_generationFault=true; return false;
        }
        DG::ObserveExisting(r);
        if(GenerationSharedVanilla(d)) {
            r.pixels=GenerationChannel(*target,d.component);
            DG::ObserveExisting(r);
            if(newlyAdded && d.generation.enabled) Logf("generation WARN [%s] shares a vanilla/terrain channel; random placement suppressed (use independent_map=1 for terrain)",d.name);
        } else if(g_desertMap && d.desertFill && ((newlyAdded && !DG::HasData(r.pixels)) || DG::Pending(r))) {
            // 0.4.1: desert_fill - on a $TYPE_DESERT map the whole land is this deposit
            // instead of random regions; only for a deposit that never held data, like the
            // first distribution. Water stays clear, so mine placement is unchanged.
            // Infinite unless the Depletion plugin mines it down.
            // 0.4.6: organic instead of flat - two octaves of value noise pick a richness
            // inside the desert_fill_min..max band, and with desert_fill_relief the richness
            // falls with terrain height: full band on the lowest land (2nd percentile),
            // nothing on the highest (98th percentile), a little noise on the slope so the
            // edge is not a contour line.
            if(r.width!=DG::Side || r.height!=DG::Side) Logf("generation [%s] desert_fill needs a 1024x1024 resource map; left empty",d.name);
            else {
                if(desertLand.empty() && !GenerationLand(terrain,size[0],size[1],offset,desertLand)) return false;
                std::vector<float> heights; float hLow=0,hHigh=0;
                if(d.desertFillRelief) {
                    if(!GenerationHeights(terrain,size[0],size[1],offset,heights)) { Logf("generation WARN [%s] terrain heights unreadable; desert fill left empty",d.name); continue; }
                    std::vector<float> land; land.reserve(DG::Cells);
                    for(size_t p=0;p<DG::Cells;++p) if(!(desertLand[p]&DG::Water)) land.push_back(heights[p]);
                    if(land.size()<16) { Logf("generation [%s] desert map: no land; nothing filled",d.name); continue; }
                    size_t lo=land.size()*2/100, hi=land.size()*98/100; if(hi>=land.size()) hi=land.size()-1;
                    std::nth_element(land.begin(),land.begin()+lo,land.end()); hLow=land[lo];
                    std::nth_element(land.begin(),land.begin()+hi,land.end()); hHigh=land[hi];
                    if(!(hHigh>hLow+0.5f)) hHigh=hLow+0.5f;
                }
                unsigned lowPct=(std::min)(d.desertFillMin,d.desertFillMax), highPct=(std::max)(d.desertFillMin,d.desertFillMax);
                float lo=lowPct/100.f, hi=highPct/100.f;
                uint32_t ns=(uint32_t)DG::Hash(d.name,strlen(d.name),candidate.seed);
                unsigned filled=0;
                for(unsigned y=0;y<DG::Side;++y) for(unsigned x=0;x<DG::Side;++x) {
                    size_t p=(size_t)y*DG::Side+x;
                    if(desertLand[p]&DG::Water) { r.pixels[p]=0; continue; }
                    float n=0.65f*DG::Noise(ns,x/110.f,y/110.f)+0.35f*DG::Noise(ns^0x5bd1e995u,x/28.f,y/28.f);
                    float band=lo+(hi-lo)*n, cover=1.f;
                    if(d.desertFillRelief) {
                        float hn=(heights[p]-hLow)/(hHigh-hLow)+0.12f*(DG::Noise(ns^0x27d4eb2fu,x/40.f,y/40.f)-0.5f);
                        cover=1.f-(std::max)(0.f,(std::min)(1.f,hn));
                    }
                    int v=(int)lroundf(255.f*band*cover); if(v<0) v=0; if(v>255) v=255;
                    r.pixels[p]=(unsigned char)v; filled+=v!=0;
                }
                r.status=1; r.placed=1;
                if(d.desertFillRelief) Logf("generation [%s] desert map: %u/%u cells hold sand; richness %u..%u%% on the lowest land (%.1f m) falling to 0 at the highest (%.1f m); desert_fill_relief=1",d.name,filled,DG::Cells,lowPct,highPct,hLow,hHigh);
                else Logf("generation [%s] desert map: %u/%u cells hold sand; richness %u..%u%% over the whole land; desert_fill_relief=0",d.name,filled,DG::Cells,lowPct,highPct);
            }
        } else if((newlyAdded && !DG::HasData(r.pixels)) || DG::Pending(r)) {
            const bool priorPending=!newlyAdded;
            r.status=2;
            const char* reason=nullptr;
            if(!g_generationEnabled) reason="global generation disabled";
            else if(!d.generation.enabled) reason="resource generation disabled";
            else if(!d.generation.count) reason="region count is zero";
            else if(!g_generateLegacyEmpty && (priorPending || (g_generationLegacy && !g_generationHadState)))
                reason="generate_existing_empty=0 protects legacy/pending empty records";
            else if(r.width!=DG::Side || r.height!=DG::Side) reason="generation requires 1024x1024 resource maps";
            if(reason) Logf("generation [%s] pending first distribution: %s",d.name,reason);
            else {
                generate.push_back(i);
                if(priorPending) {
                    ++resumed;
                    Logf("generation [%s] resuming first distribution: saved status=skipped-empty, never recorded as adopted/generated",d.name);
                }
            }
        } else if(newlyAdded) {
            Logf("generation [%s] existing resource data adopted; no random replenishment",d.name);
        }
    }
    if(!generate.empty()) {
        DG::Bytes blocked;
        if(!GenerationLand(terrain,size[0],size[1],offset,blocked)) return false;
        DG::Bytes occupied(DG::Cells,0);
        // 0.4.3: every source is counted on its own so the log tells which map blocks the land
        // (Siberia/Asia showed 89% of the land reserved although the resource maps cover 15%).
        unsigned srcCells[8]={0}; unsigned maskNonZero[4]={0};
        auto merge=[&](int slot,const DG::Bytes& pixels,unsigned w,unsigned h){DG::Bytes mask=DG::Occupancy(pixels,w,h);unsigned n=0;for(size_t i=0;i<occupied.size();++i) if(mask[i]){occupied[i]=1;++n;} srcCells[slot]=n;};
        for(const auto& m:live) {
            if(m.id==0) for(int c=0;c<3;++c) merge(c,GenerationChannel(m,c),m.w,m.h);
            if(m.id==1) for(int c=0;c<2;++c) merge(3+c,GenerationChannel(m,c),m.w,m.h);
            if(m.id==64) { if(g_generationBlockGravel) merge(5,GenerationChannel(m,2),m.w,m.h); for(int c=0;c<4;++c){DG::Bytes ch=GenerationChannel(m,c);for(unsigned char v:ch) maskNonZero[c]+=v!=0;} }
        }
        // Removed-resource tombstones also reserve their remaining footprint. A desert_fill
        // deposit on a desert map does not: it is everywhere, and the ores must still fit (0.4.6).
        for(size_t k=0;k<candidate.records.size();++k) {
            bool fillRecord=false;
            if(g_desertMap) for(int i=0;i<g_depCount;++i) if(g_dep[i].desertFill && recordIndex[i]==(int)k) fillRecord=true;
            if(fillRecord) continue;
            const auto& r=candidate.records[k]; DG::Bytes mask=DG::Occupancy(r.pixels,r.width,r.height); for(size_t i=0;i<occupied.size();++i) if(mask[i]){occupied[i]=1;++srcCells[6];}
        }
        unsigned unionCells=0; for(unsigned char v:occupied) unionCells+=v!=0;
        const GenerationMap* maskMap=GenerationFindMap(live,64);
        Logf("generation occupancy: resourcemap R=%u G=%u B=%u; resourcemap2 R=%u G=%u; mask B=%u (%s; mask %ux%u non-zero R=%u G=%u B=%u A=%u); tombstones=%u; union=%u cells before the %.0fm gap",
             srcCells[0],srcCells[1],srcCells[2],srcCells[3],srcCells[4],srcCells[5],g_generationBlockGravel?"gravel blocks new fields":"gravel ignored, generation_block_gravel=0",maskMap?maskMap->w:0,maskMap?maskMap->h:0,
             maskNonZero[0],maskNonZero[1],maskNonZero[2],maskNonZero[3],srcCells[6],unionCells,g_generationGap);
        occupied=DG::Dilate(occupied,(unsigned)ceilf(g_generationGap*DG::Side/size[0]),
                                   (unsigned)ceilf(g_generationGap*DG::Side/size[1]));
        unsigned reserved=0,eligible=0;
        for(size_t p=0;p<blocked.size();++p) {
            if(occupied[p]) { blocked[p]|=DG::Deposit; ++reserved; }
            eligible+=blocked[p]==0;
        }
        Logf("generation placement: eligible=%u/%u cells after %u deposit/gap cells; round-robin, clipping requires >=60%% of original field area",
             eligible,DG::Cells,reserved);
        std::vector<DG::Request> requests;
        for(int i:generate) requests.push_back({&candidate.records[recordIndex[i]],g_dep[i].generation});
        auto results=DG::GenerateBatch(requests,candidate.seed,blocked,size[0],size[1],g_generationGap);
        for(size_t at=0;at<generate.size();++at) {
            int i=generate[at]; const auto& result=results[at];
            bool complete=result.placed==g_dep[i].generation.count;
            Logf("generation %s [%s]: placed %u/%u clipped=%u attempts=%u rejected=%u seed=%llu",
                 complete?"OK":"WARN",g_dep[i].name,result.placed,g_dep[i].generation.count,result.clipped,
                 result.attempts,result.rejected,(unsigned long long)candidate.seed);
            if(!complete || result.rejected) Logf("generation detail [%s]: %s; rejected touching country=%u water/shore=%u deposits/gap=%u map-edge=%u; insufficient-retained-area=%u centre-now-occupied=%u oversized-geometry=%u (causes may overlap)",
                 g_dep[i].name,complete?"target reached":(result.noFreeLand?"no eligible land before placement":"attempt budget exhausted; target not fully reached with current constraints"),
                 result.country,result.water,result.deposits,result.mapEdge,result.tooSmall,result.centreBlocked,result.invalidGeometry);
        }
    }
    std::vector<GenerationMap> updated=live;
    for(int i=0;i<g_depCount;++i) {
        const auto& d=g_dep[i]; auto& r=candidate.records[recordIndex[i]];
        r.map=d.map; r.component=d.component;
        if(GenerationSharedVanilla(d)) continue;
        GenerationMap* m=GenerationFindMap(updated,d.map);
        int shift=GenerationShift(d.component); DWORD mask=255u<<shift;
        for(size_t p=0;p<m->pixels.size();++p) m->pixels[p]=(m->pixels[p]&~mask)|(DWORD(r.pixels[p])<<shift);
    }
    // Prepare everything before touching a live channel; roll back all changed
    // maps if any transfer fails. We never modify the terrain or vanilla channels.
    std::vector<size_t> changed;
    for(size_t i=0;i<live.size();++i) if(live[i].pixels!=updated[i].pixels) {
        changed.push_back(i);
        if(!GenerationTransfer(GenerationTexture(live[i].id),updated[i].pixels.data(),live[i].w,live[i].h,true)) {
            for(size_t j:changed) if(!GenerationTransfer(GenerationTexture(live[j].id),live[j].pixels.data(),live[j].w,live[j].h,true))
                Logf("generation ERROR rollback failed for map %d; reload without saving",live[j].id);
            g_generationFault=true; return false;
        }
    }
    // Removed entries must not claim a physical channel now reused by new ones.
    for(size_t i=0;i<candidate.records.size();++i)
        if(std::find(recordIndex.begin(),recordIndex.end(),(int)i)==recordIndex.end()) candidate.records[i].map=-1;
    g_generationState=std::move(candidate); g_generationReady=true;
    Logf("generation ready: %u tracked resources, %u first-generation requests (%u previously pending); existing/depleted resources preserved",
         (unsigned)g_generationState.records.size(),(unsigned)generate.size(),resumed);
    return true;
}
static bool GenerationRunGuarded(void* terrain) {
    __try { return GenerationRun(terrain); }
    __except(FaultFilter("deposit generation init",GetExceptionInformation())) { g_generationFault=true; return false; }
}
static void h_GenerationRender(void* self,bool a,void* c1,void* c2,int b,int c) {
    {
    GenerationLock lock;
    if(g_generationPending && !g_generationFault && g_generationSaveReady) {
        BYTE* game=*(BYTE**)(g_exeBase+P_GAMEOBJ);
        if(ReadablePtr(game,P_TERRAIN_OFF+8) && *(void**)(game+P_TERRAIN_OFF)==self) {
            try {
                if(GenerationRunGuarded(self)) g_generationPending=false;
                else if(++g_generationWait>=3) {
                    Logf("generation WARN initialization unavailable after 3 frames; no automatic placement/state overwrite in this world");
                    g_generationFault=true;
                }
            } catch(...) { Logf("generation WARN allocation/initialization failed; no state overwrite"); g_generationFault=true; }
        }
    }
    }
    o_GenerationRender(self,a,c1,c2,b,c);
}
static int h_GenerationTerrainInit(void* self,void* mp,char* folder,void* lighting,void* water,
                                   void* shaders,const char* shader,unsigned flags,int detail) {
    int result=o_GenerationTerrainInit(self,mp,folder,lighting,water,shaders,shader,flags,detail);
    GenerationLock lock;
    g_generationTerrain=self; g_generationWater=water;
    return result;
}
} // anonymous namespace

// 0.4.1: the map type the world declares in its script.ini ($TYPE_DESERT, $TYPE_JUNGLE,
// $TYPE_SIBERIA; the meadow maps carry none). Saved games ship the script.ini as well.
static bool GenerationMapIsDesert(const std::string& folder) {
    DG::Bytes text; if(!GenerationReadFile(folder+"\\script.ini",text,4u*1024*1024)) return false;
    std::string s(text.begin(),text.end()); size_t at=0;
    while(at<s.size()) {
        size_t end=s.find('\n',at); if(end==std::string::npos) end=s.size();
        std::string line=s.substr(at,end-at); at=end+1;
        size_t b=line.find_first_not_of(" \t\r"),e=line.find_last_not_of(" \t\r");
        if(b==std::string::npos) continue; line=line.substr(b,e-b+1);
        if(_stricmp(line.c_str(),"$TYPE_DESERT")==0) return true;
    }
    return false;
}
static void GenerationWorldLoading(const char* folder) {
    GenerationLock lock;
    g_generationReady=false; g_generationFault=false; g_generationPending=false; g_generationWait=0;
    g_generationState=DG::State(); g_generationHadState=false;
    try {
        g_generationFolder=GenerationResolveFolder(folder);
        if(g_generationFolder.empty()) { Logf("generation WARN world folder cannot be resolved: %s",folder); g_generationFault=true; return; }
        g_generationLegacy=GenerationIsSave(g_generationFolder);
        g_desertMap=GenerationMapIsDesert(g_generationFolder);
        if(g_desertMap) Logf("generation map type: desert ($TYPE_DESERT in script.ini); desert_fill deposits cover the whole land");
        std::string path=g_generationFolder+"\\tesmio_deposits.bin";
        DWORD attr=GetFileAttributesA(path.c_str());
        if(attr!=INVALID_FILE_ATTRIBUTES) {
            DG::Bytes data; std::string error;
            if(!GenerationReadFile(path,data) || !DG::Decode(data,g_generationState,error)) {
                Logf("generation WARN saved state unreadable/invalid (%s); no remap/generation/overwrite",error.c_str()); g_generationFault=true; return;
            }
            g_generationHadState=true;
        } else if(GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND) {
            Logf("generation WARN saved state inaccessible; refusing to treat it as new"); g_generationFault=true; return;
        } else {
            LARGE_INTEGER counter; QueryPerformanceCounter(&counter);
            FILETIME ft; GetSystemTimeAsFileTime(&ft);
            uint64_t entropy=(uint64_t(ft.dwHighDateTime)<<32)|ft.dwLowDateTime;
            g_generationState.seed=g_generationSeed?g_generationSeed:(entropy^(uint64_t)counter.QuadPart);
        }
        g_generationPending=true;
        Logf("generation world: %s; metadata=%s legacy=%d; waiting for terrain render",g_generationFolder.c_str(),g_generationHadState?"loaded":"new",g_generationLegacy);
        Logf("generation saved-world seed=%llu; checking all %d configured resources against this world's history",
             (unsigned long long)g_generationState.seed,g_depCount);
        if(!g_generationHadState && g_generationLegacy && g_generationEnabled && g_generateLegacyEmpty)
            Logf("generation NOTE first legacy adoption: unrecorded empty plugin channels will be initialized; without metadata historical exhaustion cannot be distinguished (generate_existing_empty=0 opts out)");
    } catch(...) { g_generationFault=true; Logf("generation WARN world initialization allocation failed"); }
}
static void GenerationSaved(const char* folder) {
    GenerationLock lock;
    if(!g_generationReady || g_generationFault || !g_generationSaveReady) return;
    try {
        std::string dest=GenerationResolveFolder(folder);
        if(dest.empty()) { Logf("generation WARN save folder unresolved; sidecar not written"); return; }
        DG::State snapshot=g_generationState;
        std::vector<GenerationMap> maps;
        for(auto& r:snapshot.records) if(r.map>=0) {
            GenerationMap* map=GenerationFindMap(maps,r.map);
            if(!map) {
                GenerationMap m;
                if(!GenerationDds(dest,r.map,m)) { Logf("generation WARN saved DDS missing/unsupported for %s; sidecar not overwritten",r.token.c_str()); return; }
                maps.push_back(std::move(m)); map=&maps.back();
            }
            if(map->w!=r.width || map->h!=r.height) { Logf("generation WARN save dimensions changed; sidecar not overwritten"); return; }
            r.pixels=GenerationChannel(*map,r.component);
            DG::ObserveExisting(r);
        }
        DG::Bytes data;
        if(!DG::Encode(snapshot,data) || !GenerationAtomicWrite(dest,data)) {
            Logf("generation WARN sidecar save failed; native maps kept, metadata not overwritten"); return;
        }
        g_generationState=std::move(snapshot);
        Logf("generation saved %u resource identities + current depletion (%u bytes): %s",
             (unsigned)g_generationState.records.size(),(unsigned)data.size(),dest.c_str());
    } catch(...) { Logf("generation WARN save allocation failed; native maps kept"); }
}
static bool InstallGeneration() {
    if(!g_depCount || !g_generationSaveReady) return false;
    #define GEN_EXPORT(var,type,name) var=(type)GetProcAddress(g_engine,name); if(!var) { Logf("generation WARN missing engine export %s; generation unavailable",name); return false; }
    GEN_EXPORT(genOffset,GenVec,"?GetOffset@C3D_TERRAIN@@QEAA?AVC3DVECTOR3@@XZ")
    GEN_EXPORT(genSize,GenVec,"?GetTerrainSize@C3D_TERRAIN@@QEAA?AVC3DVECTOR2@@XZ")
    GEN_EXPORT(genHeight,GenPointHeight,"?CollisionTerrainHeightOverPoint@C3D_TERRAIN@@QEAAMVC3DVECTOR3@@@Z")
    GEN_EXPORT(genWaterHeight,GenFloat,"?GetHeight@C3D_WATER@@QEAAMXZ")
    GEN_EXPORT(genWaterAmplitude,GenFloat,"?GetAmplitude@C3D_WATER@@QEAAMXZ")
    GEN_EXPORT(genNoWater,GenBool,"?GetNoWater@C3D_TERRAIN@@QEAA_NXZ")
    GEN_EXPORT(genHeightWidth,GenInt,"?GetHeightmapSizeX@C3D_TERRAIN@@QEAAHXZ")
    GEN_EXPORT(genHeightHeight,GenInt,"?GetHeightmapSizeZ@C3D_TERRAIN@@QEAAHXZ")
    GEN_EXPORT(genBoundaryXMin,GenFloat,"?GetTerrainBoundaryXMin@C3D_TERRAIN@@QEAAMXZ")
    GEN_EXPORT(genBoundaryZMin,GenFloat,"?GetTerrainBoundaryZMin@C3D_TERRAIN@@QEAAMXZ")
    GEN_EXPORT(genBoundaryXMax,GenFloat,"?GetTerrainBoundaryXMax@C3D_TERRAIN@@QEAAMXZ")
    GEN_EXPORT(genBoundaryZMax,GenFloat,"?GetTerrainBoundaryZMax@C3D_TERRAIN@@QEAAMXZ")
    #undef GEN_EXPORT
    genBorderLayoutReady=GenerationBorderSignature((const BYTE*)GetProcAddress(g_engine,"??0C3D_TERRAIN@@QEAA@XZ"));
    if(!genBorderLayoutReady) Logf("generation WARN country-vector layout guard failed; existing save restoration remains available, first placement disabled");
    const char* init="?InitializeFromFolder@C3D_TERRAIN@@QEAAHPEAVC3D_MIDDLEPOINT@@PEADPEAVC3D_LIGHTING@@PEAVC3D_WATER@@PEAVC3DAPI_SHADERS@@PEBDIH@Z";
    const char* render="?Render@C3D_TERRAIN@@QEAAX_NPEAVC3D_CAMERA@@0HH@Z";
    if(!FindIatSlot(g_exe,DLL_ENGINE,init)||!FindIatSlot(g_exe,DLL_ENGINE,render)) {
        Logf("generation WARN terrain init/render imports missing; no generation hooks installed"); return false;
    }
    if(!PatchIat(g_exe,DLL_ENGINE,init,(void*)h_GenerationTerrainInit,(void**)&o_GenerationTerrainInit,"generation terrain init")) return false;
    if(!PatchIat(g_exe,DLL_ENGINE,render,(void*)h_GenerationRender,(void**)&o_GenerationRender,"generation render")) return false;
    Logf("generation enabled=%d; first-time resources only, water/country excluded, country-guard=%d gap=%.1fm block_gravel=%d, first legacy empty initialization=%d",g_generationEnabled,genBorderLayoutReady,g_generationGap,g_generationBlockGravel,g_generateLegacyEmpty);
    return true;
}
