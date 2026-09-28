// Pure CPU generation and persistence. No engine calls, hooks or game writes.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace deposit_generation {
using Bytes = std::vector<unsigned char>;
static const unsigned Side = 1024;
static const unsigned Cells = Side * Side;
static const unsigned MaxRecords = 128; // includes removed-resource tombstones
struct Settings {
    bool enabled = true;
    unsigned count = 3;
    float radiusMin = 350, radiusMax = 550; // world metres, reference radius
    float richnessMin = .45f, richnessMax = 1;
};
inline bool ApplyFrequency(Settings& s,unsigned level) {
    if(level<1 || level>6) return false;
    s.count=level; return true;
}
inline bool ApplySize(Settings& s,unsigned level) {
    static const float low[]={150,350,550},high[]={350,550,750};
    if(level<1 || level>3) return false;
    s.radiusMin=low[level-1]; s.radiusMax=high[level-1]; return true;
}
struct Record {
    std::string token;
    int type = -1;
    int map = -1, component = -1; // last save's physical channel, -1 = removed
    // Persisted v1 values remain compatible. A skipped-empty entry is NOT an
    // initialized deposit. Generated/adopted entries stay initialized at zero.
    unsigned status = 0; // 0 adopted, 1 generated, 2 pending, 3 no/partial space
    unsigned placed = 0;
    unsigned width = Side, height = Side;
    Bytes pixels;
};
struct State { uint64_t seed = 0; std::vector<Record> records; };
inline uint64_t Hash(const void* p, size_t n, uint64_t h = 14695981039346656037ull) {
    const unsigned char* b = static_cast<const unsigned char*>(p);
    while(n--) { h ^= *b++; h *= 1099511628211ull; }
    return h;
}
inline uint64_t KeySeed(uint64_t seed, const std::string& key) {
    return Hash(key.data(), key.size(), seed ^ 0x9e3779b97f4a7c15ull);
}
struct Random {
    uint64_t s;
    explicit Random(uint64_t seed) : s(seed) {}
    uint64_t next() {
        uint64_t x = (s += 0x9e3779b97f4a7c15ull);
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        return x ^ (x >> 31);
    }
    float unit() { return static_cast<float>(next() >> 40) / 16777216.f; }
    float range(float a,float b) { return a + (b-a)*unit(); }
};
inline bool HasData(const Bytes& b) { for(auto v:b) if(v) return true; return false; }
inline bool Pending(const Record& r) {
    return r.status==2 && r.placed==0 && !HasData(r.pixels);
}
inline void ObserveExisting(Record& r) {
    // Keep evidence of an adopted/manual deposit when its last byte is mined.
    if(r.status==2 && HasData(r.pixels)) r.status=0;
}
inline int Find(const State& s,const std::string& token) {
    for(size_t i=0;i<s.records.size();++i) if(s.records[i].token==token) return (int)i;
    return -1;
}
inline bool ValidSettings(const Settings& s) {
    return s.count<=128 && std::isfinite(s.radiusMin) && std::isfinite(s.radiusMax) &&
        s.radiusMin>=20 && s.radiusMax>=s.radiusMin && s.radiusMax<=3000 &&
        std::isfinite(s.richnessMin) && std::isfinite(s.richnessMax) &&
        s.richnessMin>0 && s.richnessMax>=s.richnessMin && s.richnessMax<=1;
}
// Conservative resampling of an existing mask; keeps a nonzero source pixel
// occupied even when source and generation grid sizes differ.
inline Bytes Occupancy(const Bytes& src,unsigned w,unsigned h) {
    Bytes out(Cells,0);
    if(!w || !h || src.size()!=(size_t)w*h) return out;
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) if(src[(size_t)y*w+x]) {
        unsigned x0=x*Side/w, x1=((x+1)*Side+w-1)/w;
        unsigned y0=y*Side/h, y1=((y+1)*Side+h-1)/h;
        for(unsigned yy=y0;yy<y1 && yy<Side;++yy)
            for(unsigned xx=x0;xx<x1 && xx<Side;++xx) out[yy*Side+xx]=1;
    }
    return out;
}
// Chebyshev dilation is intentionally conservative (at least the requested gap).
inline Bytes Dilate(const Bytes& src,unsigned rx,unsigned ry) {
    if(src.size()!=Cells) return Bytes();
    rx=(std::min)(rx,Side); ry=(std::min)(ry,Side);
    Bytes tmp(Cells,0),dst(Cells,0);
    for(unsigned y=0;y<Side;++y) {
        int sum=0;
        for(unsigned x=0;x<(std::min)(Side,rx+1);++x) sum+=src[y*Side+x]!=0;
        for(unsigned x=0;x<Side;++x) {
            tmp[y*Side+x]=sum>0;
            if(x>=rx) sum-=src[y*Side+x-rx]!=0;
            if(x+rx+1<Side) sum+=src[y*Side+x+rx+1]!=0;
        }
    }
    for(unsigned x=0;x<Side;++x) {
        int sum=0;
        for(unsigned y=0;y<(std::min)(Side,ry+1);++y) sum+=tmp[y*Side+x]!=0;
        for(unsigned y=0;y<Side;++y) {
            dst[y*Side+x]=sum>0;
            if(y>=ry) sum-=tmp[(y-ry)*Side+x]!=0;
            if(y+ry+1<Side) sum+=tmp[(y+ry+1)*Side+x]!=0;
        }
    }
    return dst;
}
enum BlockReason : unsigned char { Water=1, Country=2, Deposit=4, MapEdge=8 };
struct Result {
    unsigned placed=0, attempts=0, rejected=0, clipped=0;
    unsigned tooSmall=0, invalidGeometry=0, centreBlocked=0;
    // Rejected attempts can touch more than one cause; not an additive total.
    unsigned water=0, country=0, deposits=0, mapEdge=0;
    bool noFreeLand=false;
};
inline void Rejected(Result& r,unsigned causes) {
    ++r.rejected;
    r.water+=(causes&Water)!=0; r.country+=(causes&Country)!=0;
    r.deposits+=(causes&Deposit)!=0; r.mapEdge+=(causes&MapEdge)!=0;
}
inline float Smooth(float low,float high,float v) {
    float t=(std::max)(0.f,(std::min)(1.f,(v-low)/(high-low)));
    return t*t*(3-2*t);
}
inline float Lattice(uint32_t seed,int x,int y) {
    uint32_t h=seed ^ (uint32_t(x)*0x9e3779b9u) ^ (uint32_t(y)*0x85ebca6bu);
    h^=h>>16; h*=0x7feb352du; h^=h>>15; h*=0x846ca68bu; h^=h>>16;
    return float(h>>8)/16777216.f;
}
inline float Noise(uint32_t seed,float x,float y) {
    int ix=(int)floorf(x),iy=(int)floorf(y);
    float fx=Smooth(0,1,x-ix),fy=Smooth(0,1,y-iy);
    float a=Lattice(seed,ix,iy),b=Lattice(seed,ix+1,iy);
    float c=Lattice(seed,ix,iy+1),d=Lattice(seed,ix+1,iy+1);
    return (a+(b-a)*fx)*(1-fy)+(c+(d-c)*fx)*fy;
}
struct NaturalShape {
    uint32_t noise;
    float length,width,bend,phase,angle,branchSide,branchStart;
    unsigned gaps;
    float gapPosition[2],gapWidth[2];
};
inline NaturalShape MakeNaturalShape(Random& rng) {
    NaturalShape s={}; s.noise=(uint32_t)rng.next();
    s.length=rng.range(1.7f,2.3f); s.width=rng.range(.85f,1.1f);
    s.bend=rng.range(-.4f,.4f); s.phase=rng.range(0,6.283185307f);
    s.angle=rng.range(0,6.283185307f);
    s.branchSide=rng.unit()<.55f?(rng.unit()<.5f?-1.f:1.f):0.f;
    s.branchStart=rng.range(-.6f,-.15f);
    s.gaps=rng.unit()<.3f?2:1;
    s.gapPosition[0]=s.gaps==2?rng.range(-.55f,-.25f):rng.range(-.5f,.5f);
    s.gapPosition[1]=rng.range(.25f,.55f);
    for(unsigned i=0;i<2;++i) s.gapWidth[i]=rng.range(.035f,.07f);
    return s;
}
// A whole region is sampled BEFORE it claims occupancy, so branches may join
// each other. Holes/gaps are actual zero-richness cells, not display effects.
inline float NaturalDensity(const NaturalShape& shape,float u,float v,float cellScale) {
    float s=u/shape.length;
    if(fabsf(s)>=1) return 0;
    float curve=shape.bend*sinf(u*1.65f+shape.phase)+.10f*sinf(u*3.1f-shape.phase);
    float width=shape.width*sqrtf((std::max)(0.f,1-s*s));
    width*=.65f+.55f*Noise(shape.noise,u*2.1f+.31f,v*1.7f+.67f);
    float warpedV=v+.16f*(Noise(shape.noise+1,u*3.0f,v*3.0f)*2-1);
    float edge=1-fabsf(warpedV-curve)/(std::max)(.001f,width);
    if(shape.branchSide!=0 && s>shape.branchStart && s<.9f) {
        float t=(s-shape.branchStart)/(.9f-shape.branchStart);
        float bulge=sinf(t*3.141592654f);
        float centre=curve+shape.branchSide*(.9f*bulge);
        float branchWidth=shape.width*.46f*bulge;
        edge=(std::max)(edge,1-fabsf(warpedV-centre)/(std::max)(.001f,branchWidth));
    }
    if(edge<=0) return 0;
    float density=Smooth(0,.45f,edge)*Smooth(0,.16f,1-fabsf(s));
    // Coherent erosion creates broad ragged edges/holes, not pixel noise.
    float nu=u+.24f*(Noise(shape.noise+11,u*1.9f+2.1f,v*1.9f)*2-1);
    float nv=v+.24f*(Noise(shape.noise+12,u*1.9f,v*1.9f-3.7f)*2-1);
    density*=Smooth(.23f,.62f,Noise(shape.noise+2,nu*3.4f+5.3f,nv*3.4f-4.8f));
    float gapWarp=.14f*sinf(v*2.6f+shape.phase)+.06f*(Noise(shape.noise+3,u*2,v*2)*2-1);
    for(unsigned i=0;i<shape.gaps;++i) {
        float halfWidth=shape.gapWidth[i]*(.7f+.8f*Noise(shape.noise+5+i,u*2,v*2));
        halfWidth=(std::max)(halfWidth,.75f*cellScale/shape.length);
        density*=Smooth(halfWidth,halfWidth+.045f,fabsf(s+gapWarp-shape.gapPosition[i]));
    }
    density*=.76f+.24f*Noise(shape.noise+4,u*1.3f+.8f,v*1.3f+.4f);
    return density;
}
struct PatchCell { unsigned index; unsigned char value; };
inline void RemoveSpecks(std::vector<PatchCell>& patch,int x0,int y0,int w,int h) {
    // Remove isolated raster specks, keeping real small satellite deposits.
    std::vector<int> lookup((size_t)w*h,-1),queue;
    for(size_t i=0;i<patch.size();++i) {
        int x=(int)(patch[i].index%Side)-x0,y=(int)(patch[i].index/Side)-y0;
        lookup[y*w+x]=(int)i;
    }
    unsigned minimum=(std::max)(2u,(unsigned)patch.size()/200u);
    for(size_t i=0;i<patch.size();++i) {
        int x=(int)(patch[i].index%Side)-x0,y=(int)(patch[i].index/Side)-y0;
        if(lookup[y*w+x]<0) continue;
        queue.clear(); queue.push_back((int)i); lookup[y*w+x]=-1;
        for(size_t at=0;at<queue.size();++at) {
            auto index=patch[queue[at]].index;
            int px=(int)(index%Side)-x0,py=(int)(index/Side)-y0;
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                int nx=px+dx,ny=py+dy;
                if(nx<0 || ny<0 || nx>=w || ny>=h) continue;
                int& n=lookup[ny*w+nx]; if(n>=0) { queue.push_back(n); n=-1; }
            }
        }
        if(queue.size()<minimum) for(int n:queue) patch[n].value=0;
    }
    patch.erase(std::remove_if(patch.begin(),patch.end(),[](const PatchCell& c){return c.value==0;}),patch.end());
}
struct Request { Record* record; Settings settings; };
struct Placement {
    Request request; Random rng; Result result;
    Placement(const Request& q,uint64_t seed):request(q),rng(KeySeed(seed,q.record->token)){}
};
inline bool PlaceOne(Placement& work,Bytes& blocked,const std::vector<unsigned>& centres,
                     float sizeX,float sizeZ,int gx,int gy,std::vector<PatchCell>& patch) {
    auto& result=work.result; const auto& cfg=work.request.settings; auto& rng=work.rng;
    auto& rec=*work.request.record;
    // One turn gets at most one region and 32 attempts. All materials get a turn
    // before another material can claim its next region. Failed turns are bounded.
    const unsigned stop=(std::min)(cfg.count*256,result.attempts+32);
    while(result.attempts<stop) {
        ++result.attempts;
        const unsigned centre=centres[(size_t)(rng.next()%centres.size())];
        if(blocked[centre]) { ++result.centreBlocked; Rejected(result,blocked[centre]); continue; }
        const float cx=float(centre%Side)+rng.unit(),cy=float(centre/Side)+rng.unit();
        const float radius=rng.range(cfg.radiusMin,cfg.radiusMax);
        NaturalShape shape=MakeNaturalShape(rng);
        const float rich=rng.range(cfg.richnessMin,cfg.richnessMax);
        const float co=cosf(shape.angle),si=sinf(shape.angle);
        const float ex=(fabsf(co)*shape.length+fabsf(si)*2.1f)*radius*Side/sizeX;
        const float ey=(fabsf(si)*shape.length+fabsf(co)*2.1f)*radius*Side/sizeZ;
        if(!std::isfinite(ex) || !std::isfinite(ey) || ex>Side || ey>Side) {
            ++result.invalidGeometry; Rejected(result,MapEdge); continue;
        }
        const int x0=(int)floorf(cx-ex),x1=(int)ceilf(cx+ex);
        const int y0=(int)floorf(cy-ey),y1=(int)ceilf(cy+ey);
        const float cellScale=(std::max)(sizeX,sizeZ)/(Side*radius);
        unsigned total=0,causes=0; patch.clear();
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            float dx=(x+.5f-cx)*sizeX/(Side*radius),dy=(y+.5f-cy)*sizeZ/(Side*radius);
            float density=NaturalDensity(shape,co*dx+si*dy,-si*dx+co*dy,cellScale);
            unsigned value=(unsigned)(255*rich*density);
            if(!value) continue;
            ++total;
            if(x<1 || y<1 || x>=int(Side)-1 || y>=int(Side)-1) { causes|=MapEdge; continue; }
            unsigned index=y*Side+x;
            if(blocked[index]) { causes|=blocked[index]; continue; }
            patch.push_back({index,(unsigned char)(std::min)(255u,value)});
        }
        const int bx=(std::max)(0,x0),by=(std::max)(0,y0);
        if(!patch.empty()) RemoveSpecks(patch,bx,by,(std::min)(int(Side)-1,x1)-bx+1,
                                                    (std::min)(int(Side)-1,y1)-by+1);
        // Keep the chosen size meaningful: at least 60% of its generated area
        // must survive clipping/speck removal. Never count a tiny leftover as large.
        if(patch.size()<4 || patch.size()*5<total*3) {
            ++result.tooSmall; Rejected(result,causes); continue;
        }
        for(const auto& p:patch) rec.pixels[p.index]=p.value;
        // Only the small accepted patch's neighbourhood needs updating.
        for(const auto& p:patch) {
            int x=(int)(p.index%Side),y=(int)(p.index/Side);
            for(int yy=(std::max)(0,y-gy);yy<=(std::min)((int)Side-1,y+gy);++yy)
                for(int xx=(std::max)(0,x-gx);xx<=(std::min)((int)Side-1,x+gx);++xx)
                    blocked[yy*Side+xx]|=Deposit;
        }
        result.clipped+=causes!=0;
        ++result.placed;
        return true;
    }
    return false;
}
inline std::vector<Result> GenerateBatch(const std::vector<Request>& requests,uint64_t seed,
                                        Bytes& blocked,float sizeX,float sizeZ,float gap) {
    std::vector<Result> results(requests.size());
    if(blocked.size()!=Cells || !std::isfinite(sizeX) || !std::isfinite(sizeZ) ||
       sizeX<=0 || sizeZ<=0 || !std::isfinite(gap) || gap<0) return results;
    // Validate the whole batch before changing any target or the occupancy mask.
    for(size_t i=0;i<requests.size();++i) {
        const auto& q=requests[i]; if(!q.record || !ValidSettings(q.settings)) return results;
        for(size_t j=0;j<i;++j) if(q.record==requests[j].record ||
            q.record->token==requests[j].record->token) return results;
    }
    std::vector<unsigned> centres;
    for(unsigned y=1;y<Side-1;++y) for(unsigned x=1;x<Side-1;++x)
        if(!blocked[y*Side+x]) centres.push_back(y*Side+x);
    std::vector<Placement> work; std::vector<size_t> order;
    for(size_t i=0;i<requests.size();++i) {
        work.emplace_back(requests[i],seed); order.push_back(i);
        auto& r=*requests[i].record; r.width=r.height=Side; r.pixels.assign(Cells,0);
        work.back().result.noFreeLand=centres.empty();
    }
    // Seeded order, independent of INI section order. Rotate the first turn each
    // round; neither alphabetical names nor a high frequency confer precedence.
    std::sort(order.begin(),order.end(),[&](size_t a,size_t b) {
        auto ha=KeySeed(seed,requests[a].record->token),hb=KeySeed(seed,requests[b].record->token);
        return ha==hb?requests[a].record->token<requests[b].record->token:ha<hb;
    });
    const int gx=(int)(std::min)(float(Side),ceilf(gap*Side/sizeX));
    const int gy=(int)(std::min)(float(Side),ceilf(gap*Side/sizeZ));
    std::vector<PatchCell> patch;
    for(size_t round=0;!centres.empty() && !order.empty();++round) {
        bool active=false;
        for(size_t at=0;at<order.size();++at) {
            auto& w=work[order[(at+round)%order.size()]]; const auto& cfg=w.request.settings;
            if(!cfg.enabled || w.result.placed>=cfg.count || w.result.attempts>=cfg.count*256) continue;
            active=true; PlaceOne(w,blocked,centres,sizeX,sizeZ,gx,gy,patch);
        }
        if(!active) break;
    }
    for(size_t i=0;i<work.size();++i) {
        results[i]=work[i].result; auto& r=*requests[i].record;
        r.placed=results[i].placed;
        r.status=!requests[i].settings.enabled?2:(r.placed==requests[i].settings.count?1:3);
    }
    return results;
}
inline Result Generate(Record& rec,const Settings& cfg,uint64_t seed,
                       Bytes& blocked,float sizeX,float sizeZ,float gap) {
    return GenerateBatch({{&rec,cfg}},seed,blocked,sizeX,sizeZ,gap)[0];
}

inline void U32(Bytes& b,uint32_t v) { for(int i=0;i<4;++i) b.push_back((unsigned char)(v>>(i*8))); }
inline void U64(Bytes& b,uint64_t v) { U32(b,(uint32_t)v); U32(b,(uint32_t)(v>>32)); }
struct Reader {
    const Bytes& b; size_t pos=0; bool ok=true;
    uint32_t u32() {
        if(pos+4>b.size()) { ok=false; return 0; }
        uint32_t v=0; for(int i=0;i<4;++i) v|=uint32_t(b[pos++])<<(i*8); return v;
    }
    uint64_t u64() { uint64_t lo=u32(),hi=u32(); return lo|(hi<<32); }
};
inline bool ValidRecord(const Record& r) {
    if(r.token.empty() || r.token.size()>63 || r.type<10 || r.type>127 || r.status>3 ||
       !r.width || !r.height || r.width>4096 || r.height>4096 ||
       r.pixels.size()!=(size_t)r.width*r.height || r.component<0 || r.component>3 ||
       (r.map!=-1 && r.map!=64 && (r.map<0 || r.map>9))) return false;
    for(char c:r.token) if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                           (c>='0'&&c<='9')||c=='_'||c=='$')) return false;
    return true;
}
inline bool Encode(const State& state,Bytes& out) {
    if(state.records.size()>MaxRecords) return false;
    Bytes data; U64(data,state.seed); U32(data,(uint32_t)state.records.size());
    for(size_t index=0;index<state.records.size();++index) {
        const auto& r=state.records[index];
        if(!ValidRecord(r)) return false;
        for(size_t j=0;j<index;++j) if(state.records[j].token==r.token ||
            (r.map>=0 && state.records[j].map==r.map && state.records[j].component==r.component)) return false;
        U32(data,(uint32_t)r.token.size()); data.insert(data.end(),r.token.begin(),r.token.end());
        U32(data,r.type); U32(data,r.map); U32(data,r.component); U32(data,r.status);
        U32(data,r.placed); U32(data,r.width); U32(data,r.height);
        Bytes runs;
        for(size_t i=0;i<r.pixels.size();) {
            size_t n=1; while(i+n<r.pixels.size() && n<65535 && r.pixels[i+n]==r.pixels[i]) ++n;
            runs.push_back((unsigned char)n); runs.push_back((unsigned char)(n>>8));
            runs.push_back(r.pixels[i]); i+=n;
        }
        U32(data,(uint32_t)runs.size()); data.insert(data.end(),runs.begin(),runs.end());
        if(data.size()>256u*1024*1024) return false;
    }
    out.clear(); U32(out,0x47445354); U32(out,1); U64(out,Hash(data.data(),data.size()));
    out.insert(out.end(),data.begin(),data.end()); return true;
}
inline bool Decode(const Bytes& data,State& out,std::string& error) {
    if(data.size()<28 || data.size()>256u*1024*1024) { error="invalid state size"; return false; }
    Reader rd{data};
    if(rd.u32()!=0x47445354 || rd.u32()!=1) { error="unknown state format"; return false; }
    uint64_t hash=rd.u64();
    if(hash!=Hash(data.data()+16,data.size()-16)) { error="state checksum mismatch"; return false; }
    State candidate; candidate.seed=rd.u64(); unsigned count=rd.u32();
    if(count>MaxRecords) { error="too many saved resources"; return false; }
    size_t allocated=0;
    for(unsigned i=0;i<count;++i) {
        Record r; unsigned len=rd.u32();
        if(!rd.ok || !len || len>63 || rd.pos+len>data.size()) { error="invalid resource identity"; return false; }
        r.token.assign((const char*)data.data()+rd.pos,len); rd.pos+=len;
        r.type=(int)rd.u32(); r.map=(int)rd.u32(); r.component=(int)rd.u32(); r.status=rd.u32();
        r.placed=rd.u32(); r.width=rd.u32(); r.height=rd.u32(); unsigned bytes=rd.u32();
        size_t cells=(size_t)r.width*r.height;
        if(!rd.ok || !r.width || !r.height || r.width>4096 || r.height>4096 ||
           bytes%3 || rd.pos+bytes>data.size() || (allocated+=cells)>256u*1024*1024) {
            error="invalid resource payload"; return false;
        }
        r.pixels.reserve(cells);
        size_t end=rd.pos+bytes;
        while(rd.pos<end) {
            unsigned n=data[rd.pos]|(unsigned(data[rd.pos+1])<<8); unsigned char value=data[rd.pos+2]; rd.pos+=3;
            if(!n || n>cells-r.pixels.size()) { error="invalid resource run"; return false; }
            r.pixels.insert(r.pixels.end(),n,value);
        }
        if(!ValidRecord(r) || Find(candidate,r.token)>=0) { error="invalid/duplicate resource record"; return false; }
        for(const auto& old:candidate.records) if(r.map>=0 && old.map==r.map && old.component==r.component) {
            error="duplicate saved channel"; return false;
        }
        candidate.records.push_back(r);
    }
    if(!rd.ok || rd.pos!=data.size()) { error="trailing/truncated state"; return false; }
    out=std::move(candidate); return true;
}
} // namespace deposit_generation
