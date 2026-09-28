// Optional sand surface. No terrain/resource writes, no save format changes.
#include "deposit_visual_shader.h"
#include <d3d11.h>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"dxguid.lib")
namespace {
namespace VSand=DepositVisualShader;
using Microsoft::WRL::ComPtr;
static bool vsEnabled=false,vsFault=false,vsLogged=false,vsAssetDirLogged=false;
static bool vsSelectionLogged=false;
static void* vsSelectionTerrain=nullptr;
static int vsSelectionSeason=-1,vsSelectionTransition=-1,vsSelectionKinds=-1;
static float vsStrength=1.0f;
static int vsDeposit=-1;
static unsigned vsPrograms=0;
static unsigned vsPsSeen=0,vsPsMatched=0,vsPsLinked=0;
static thread_local bool vsCreating=false;
static SRWLOCK vsCreateLock=SRWLOCK_INIT;
static void** vsDeviceSlot=nullptr;
typedef int(*VsLoad)(void*,const char*);
typedef void(*VsRender)(void*,bool,void*,bool,int,int);
typedef HRESULT(STDMETHODCALLTYPE*VsCreatePS)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11PixelShader**);
typedef void*(*VsGetMaterial)(void*);
typedef void*(*VsGetTexture)(void*,unsigned);
typedef char*(*VsTextureName)(void*);
typedef ID3D11Resource*(*VsTextureResource)(void*);
static VsLoad vsOriginalLoad=nullptr;
static VsRender vsOriginalRender=nullptr;
static VsCreatePS vsOriginalPS=nullptr;
static VsGetMaterial vsMaterials[3]={};
static VsGetTexture vsTexture=nullptr;
static VsTextureName vsTextureName=nullptr;
static VsTextureResource vsResource=nullptr;
static ComPtr<ID3D11Device> vsDevice;
static ComPtr<ID3D11DeviceContext> vsContext;
static ComPtr<ID3D11ShaderResourceView> vsMapView;
// 0.4.1: one colour/normal pair per [sand_tile:] entry, loaded with the device.
static ComPtr<ID3D11ShaderResourceView> vsTileColor[MAX_SAND_TILES],vsTileNormal[MAX_SAND_TILES];
static bool vsTileReady[MAX_SAND_TILES]={};
static int vsTilesReady=0;
static ComPtr<ID3D11Resource> vsMapResource;
static ComPtr<ID3D11Buffer> vsBuffer;
static ComPtr<ID3D11SamplerState> vsSampler;
// All surface PS hashes from the installed 1.1.1.9 default_terrain.inix.
// Unknown programs (including other mods' shaders) are passed through verbatim.
static const struct {unsigned size;const char*hash;} vsKnown[]={
 {17324,"fc16d5c5e1dc9888f564eadf2ce5d6c2"},{12040,"14e9c40118146a294bfd3ab7755a25c9"},
 {17280,"02883b11ee5df7cbd9ddf058b03e82c7"},{31588,"491a6af2af015ee55f505bd078d4ef8e"},
 {33700,"5fa946abc55934e6c3077af939d2f3ad"},{14360,"2e0c83470318fa971dab9045955cf29c"},
 {14252,"fc0d9c59e6882abcc223700d86f5dc68"},{31592,"a540b8313319f4eda0124a71c61a516a"},
 {43984,"0b1b62cb1019cf53eba2c8b93176ddd9"},{16016,"7e32fcfd1a819cc98840d78a76b9d0de"},
 {14620,"49db0c9f0226a60532ac80b37e26a7a2"},{8948,"7c526a7bf45c424bc10632035c08b83b"},
 {9284,"b98b08fcd4caa31e2397f004856c9a18"}
};
static int VsKnown(const void*data,size_t n){
 if(n<32||!data||memcmp(data,"DXBC",4))return -1;
 const BYTE*b=(const BYTE*)data;char h[33];
 for(int j=0;j<16;++j)sprintf_s(h+2*j,3,"%02x",b[j+4]);
 for(int i=0;i<13;++i)if(vsKnown[i].size==n&&!strcmp(h,vsKnown[i].hash))return i;
 return -1;
}
static HRESULT STDMETHODCALLTYPE h_VsCreatePS(ID3D11Device*device,const void*data,SIZE_T n,ID3D11ClassLinkage*link,ID3D11PixelShader**shader){
 int index=vsCreating?VsKnown(data,n):-1;
 if(vsCreating){++vsPsSeen;if(index>=0)++vsPsMatched;if(link)++vsPsLinked;}
 // WRSR's native FX loader supplies a ClassLinkage object even for these
 // ordinary shaders without interfaces. Preserve it, do not reject it.
 // Exact bytecode whitelist + Patch validation still gate every augmentation.
 if(index>=0 && !vsFault){
  try {VSand::Result patch;
   if(VSand::Patch(data,n,patch)){
    HRESULT hr=vsOriginalPS(device,patch.bytes.data(),patch.bytes.size(),link,shader);
    if(SUCCEEDED(hr)){vsPrograms|=1u<<index;return hr;}
    Logf("sand surface WARN GPU rejected augmented shader %d (0x%08lX); original shader used",index,hr);
   }else Logf("sand surface WARN shader %d refused: %s; original shader used",index,patch.reason.c_str());
  }catch(...){Logf("sand surface WARN shader preparation failed; original shader used");}
  vsFault=true;
 }
 return vsOriginalPS(device,data,n,link,shader);
}
static bool VsSwap(void**slot,void*expected,void*value){
 DWORD old;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return false;
 bool ok=InterlockedCompareExchangePointer(slot,value,expected)==expected;
 DWORD ignored;VirtualProtect(slot,sizeof(void*),old,&ignored);return ok;
}
static int VsLoadCall(void*self,const char*path,void**slot){
 int r=0;vsCreating=true;
 __try {r=vsOriginalLoad(self,path);}
 __finally {vsCreating=false;if(!VsSwap(slot,(void*)h_VsCreatePS,(void*)vsOriginalPS)){
  vsFault=true;Logf("sand surface WARN pixel shader hook restore conflict; visuals disabled");}
  ReleaseSRWLockExclusive(&vsCreateLock);
 }
 return r;
}
static const char* VsBaseName(const char*p){const char*r=p;for(;*p;++p)if(*p=='/'||*p=='\\')r=p+1;return r;}
static int h_VsLoad(void*self,const char*path){
 if(!vsEnabled||vsFault||vsCreating||!path||_stricmp(VsBaseName(path),"default_terrain.ini"))return vsOriginalLoad(self,path);
 ID3D11Device*device=(ID3D11Device*)*vsDeviceSlot;
 if(!ReadablePtr(device,sizeof(void*))){vsFault=true;Logf("sand surface WARN D3D11 device unavailable; native terrain retained");return vsOriginalLoad(self,path);}
 void**table=*(void***)device;
 if(!ReadablePtr(table,16*sizeof(void*))){vsFault=true;return vsOriginalLoad(self,path);}
 AcquireSRWLockExclusive(&vsCreateLock);
 vsPrograms=0;
 vsSelectionLogged=false;
 vsPsSeen=vsPsMatched=vsPsLinked=0;
 vsOriginalPS=(VsCreatePS)table[15];
 if(!VsSwap(table+15,(void*)vsOriginalPS,(void*)h_VsCreatePS)){
  ReleaseSRWLockExclusive(&vsCreateLock);vsFault=true;Logf("sand surface WARN shader hook unavailable; native terrain retained");return vsOriginalLoad(self,path);
 }
 int r=VsLoadCall(self,path,table+15);
 if(r!=0){vsFault=true;Logf("sand surface WARN native CreateShaders returned %d; visuals disabled",r);}
 else if(vsPrograms!=0x1fff){vsFault=true;Logf("sand surface WARN expected 13 known programs, obtained mask=0x%X; PS calls=%u matched=%u linked=%u; visuals disabled",vsPrograms,vsPsSeen,vsPsMatched,vsPsLinked);}
 else Logf("sand surface shader preparation: 13/13 verified native programs augmented; PS calls=%u matched=%u linked=%u; disk shaders unchanged",vsPsSeen,vsPsMatched,vsPsLinked);
 return r;
}
static bool VsAssetPathOk(const char*p){if(!p[0]||p[0]=='/'||p[0]=='\\'||strchr(p,':'))return false;for(const char*s=p;*s;++s)if(s[0]=='.'&&s[1]=='.'&&(s==p||s[-1]=='/'||s[-1]=='\\')&&(s[2]==0||s[2]=='/'||s[2]=='\\'))return false;return true;}
static bool VsReadFile(const char*name,VSand::Bytes&bytes){
 // The assets live beside the DLL first (a Workshop package carries
 // hooks\deposits_plus\assets), then under the loader's plugins folder - the
 // local copy for anyone who keeps everything in tesmioloader\build.
 char p[MAX_PATH];HANDLE h=INVALID_HANDLE_VALUE;
 if(g_selfDir[0]&&_snprintf_s(p,sizeof(p),_TRUNCATE,"%s\\deposits_plus\\assets\\%s",g_selfDir,name)>=0)
  h=CreateFileA(p,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
 if(h==INVALID_HANDLE_VALUE){
  if(_snprintf_s(p,sizeof(p),_TRUNCATE,"%s\\plugins\\deposits_plus\\assets\\%s",g_baseDir,name)<0)return false;
  h=CreateFileA(p,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
 }
 if(!vsAssetDirLogged){vsAssetDirLogged=true;char dir[MAX_PATH];strncpy_s(dir,p,_TRUNCATE);char*slash=strrchr(dir,'\\');if(slash)*slash=0;Logf("sand surface assets from %s",dir);}
 LARGE_INTEGER n;bool ok=GetFileSizeEx(h,&n)&&n.QuadPart>0&&n.QuadPart<=48*1024*1024;   // 0.4.1: room for 2048 and 4096 tiles
 if(ok){try{bytes.resize((size_t)n.QuadPart);}catch(...){CloseHandle(h);throw;}DWORD got=0;ok=ReadFile(h,bytes.data(),(DWORD)bytes.size(),&got,nullptr)&&got==bytes.size();}
 CloseHandle(h);return ok;
}
static bool VsDds(ID3D11Device*d,const char*name,unsigned fourcc,ComPtr<ID3D11ShaderResourceView>&view){
 VSand::Bytes b;if(!VsReadFile(name,b)||b.size()<128||memcmp(b.data(),"DDS ",4)||VSand::Read(b.data()+4)!=124||
  VSand::Read(b.data()+76)!=32||VSand::Read(b.data()+84)!=fourcc)return false;
 // 0.4.1: any square power-of-two side from 256 to 4096 with a complete mip chain (1024 and 2048 in practice).
 unsigned side=VSand::Read(b.data()+16),mips=VSand::Read(b.data()+28),levels=0;
 if(VSand::Read(b.data()+12)!=side||side<256||side>4096||(side&(side-1)))return false;
 for(unsigned s=side;s;s>>=1)++levels;
 if(mips!=levels)return false;
 unsigned block=fourcc==0x31545844?8:16;size_t p=128;D3D11_SUBRESOURCE_DATA sub[13]={};
 for(unsigned m=0,w=side;m<levels;++m,w=(std::max)(1u,w/2)){
  unsigned blocks=(std::max)(1u,(w+3)/4),size=blocks*blocks*block;if(size>b.size()-p)return false;
  sub[m].pSysMem=b.data()+p;sub[m].SysMemPitch=blocks*block;p+=size;
 }
 if(p!=b.size())return false;
 D3D11_TEXTURE2D_DESC t={};t.Width=t.Height=side;t.MipLevels=levels;t.ArraySize=1;t.SampleDesc.Count=1;
 t.Format=block==8?DXGI_FORMAT_BC1_UNORM:DXGI_FORMAT_BC3_UNORM;t.Usage=D3D11_USAGE_IMMUTABLE;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 ComPtr<ID3D11Texture2D>tex;return SUCCEEDED(d->CreateTexture2D(&t,sub,&tex))&&SUCCEEDED(d->CreateShaderResourceView(tex.Get(),nullptr,&view));
}
static bool VsResources(ID3D11Device*device){
 if(vsDevice.Get()==device && vsBuffer)return true;
 for(int t=0;t<MAX_SAND_TILES;++t){vsTileColor[t].Reset();vsTileNormal[t].Reset();vsTileReady[t]=false;}vsTilesReady=0;
 vsMapView.Reset();vsMapResource.Reset();vsBuffer.Reset();vsSampler.Reset();vsContext.Reset();vsDevice=device;
 device->GetImmediateContext(&vsContext);
 // 0.4.1: every [sand_tile:] pair; a missing or malformed pair only leaves its base texture native.
 for(int t=0;t<g_tileCount;++t){
  vsTileReady[t]=VsDds(device,g_tiles[t].color,0x31545844,vsTileColor[t])&&VsDds(device,g_tiles[t].normal,0x35545844,vsTileNormal[t]);
  if(vsTileReady[t])++vsTilesReady;
  else{vsTileColor[t].Reset();vsTileNormal[t].Reset();Logf("sand surface WARN tile %s: %s / %s missing or not a square DXT1 + DXT5 pair with a full mip chain; base %s stays native",g_tiles[t].id,g_tiles[t].color,g_tiles[t].normal,g_tiles[t].base);}
 }
 if(!vsTilesReady){Logf("sand surface WARN no usable tile pair; native terrain retained");return false;}
 Logf("sand surface tiles ready: %d of %d",vsTilesReady,g_tileCount);
 D3D11_BUFFER_DESC b={};b.ByteWidth=32;b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 float zero[8]={};D3D11_SUBRESOURCE_DATA init={};init.pSysMem=zero;
 if(FAILED(device->CreateBuffer(&b,&init,&vsBuffer)))return false;
 D3D11_SAMPLER_DESC s={};s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
 s.ComparisonFunc=D3D11_COMPARISON_NEVER;s.MaxLOD=D3D11_FLOAT32_MAX;
 return SUCCEEDED(device->CreateSamplerState(&s,&vsSampler));
}
// 0.4.1: which [sand_tile:] entry belongs to a material's base texture (slot 5), by the
// path the engine holds for it with its folder, matched as a whole path element from the
// end. 0 = no entry, the base stays native (snow, desert, another mod's material).
static void VsCanonical(const char*in,char*out,size_t n){size_t i=0;for(;in[i]&&i+1<n;++i){char c=in[i];out[i]=c=='\\'?'/':(c>='A'&&c<='Z')?(char)(c+32):c;}out[i]=0;}
static int VsTile(void*material){
 if(!material)return 0;void*texture=vsTexture(material,5);if(!texture)return 0;
 char name[256];if(!SafeReadStr(vsTextureName(texture),name,sizeof(name)))return 0;
 char have[256];VsCanonical(name,have,sizeof(have));size_t hl=strlen(have);
 for(int t=0;t<g_tileCount;++t){
  char want[128];VsCanonical(g_tiles[t].base,want,sizeof(want));size_t wl=strlen(want);
  if(!wl||wl>hl)continue;
  if(strcmp(have+hl-wl,want)==0&&(hl==wl||have[hl-wl-1]=='/'))return t+1;
 }
 return 0;
}
static void VsLayerKinds(void*terrain,int season,int transition,int(&kind)[2]){
 kind[0]=kind[1]=0;
 // Native Render RVA F76B9..F7825: 1=GetMaterial (140),
 // 2=GetMaterialFall (150), 3=GetMaterialWinter (148).
 // Transitions are summer->fall, fall->snow, snow->summer.
 if(transition>=1&&transition<=3){kind[0]=VsTile(vsMaterials[transition-1](terrain));kind[1]=VsTile(vsMaterials[transition%3](terrain));}
 else if(transition==0&&season>=1&&season<=3)kind[0]=VsTile(vsMaterials[season-1](terrain));
}
static const char* VsKindName(int kind){return kind>0&&kind<=g_tileCount?g_tiles[kind-1].id:"native";}
struct VsBinding {
 ComPtr<ID3D11ShaderResourceView> views[VSand::ResourceCount];ComPtr<ID3D11Buffer> buffer;ComPtr<ID3D11SamplerState> sampler;bool bound=false;
 bool Begin(void*terrain,bool materials,int season,int transition){
  if(!vsEnabled||vsFault||vsPrograms!=0x1fff||!materials||vsDeposit<0)return false;
  BYTE*game=*(BYTE**)(g_exeBase+P_GAMEOBJ);
  if(!ReadablePtr(game,P_TERRAIN_OFF+8)||*(void**)(game+P_TERRAIN_OFF)!=terrain)return false;
  int kinds[2];VsLayerKinds(terrain,season,transition,kinds);
  float flags[2]={kinds[0]?1.f:0.f,kinds[1]?1.f:0.f};
  int selection=kinds[0]*32+kinds[1];
  if(!vsSelectionLogged||vsSelectionTerrain!=terrain||vsSelectionSeason!=season||vsSelectionTransition!=transition||vsSelectionKinds!=selection){
   char names[3][256]={};
   for(int i=0;i<3;++i){void*m=vsMaterials[i](terrain);void*t=m?vsTexture(m,5):nullptr;
    if(!t||!SafeReadStr(vsTextureName(t),names[i],sizeof(names[i])))strcpy_s(names[i],"unavailable");}
   vsSelectionLogged=true;vsSelectionTerrain=terrain;vsSelectionSeason=season;vsSelectionTransition=transition;vsSelectionKinds=selection;
   Logf("sand surface terrain selection: season=%d transition=%d surface=%s/%s grass_flags=%.0f/%.0f base_textures(summer/fall/snow)=%s|%s|%s",season,transition,VsKindName(kinds[0]),VsKindName(kinds[1]),flags[0],flags[1],VsBaseName(names[0]),VsBaseName(names[1]),VsBaseName(names[2]));
  }
  if(!flags[0]&&!flags[1])return false;
  void*map=DepositMapTexture(&g_dep[vsDeposit]);if(!map)return false;
  ID3D11Resource*resource=vsResource(map);if(!resource)return false;
  ComPtr<ID3D11Device>device;resource->GetDevice(&device);
  if(!VsResources(device.Get())){vsFault=true;Logf("sand surface WARN DDS/GPU resources unavailable; native terrain retained");return false;}
  if(resource!=vsMapResource.Get()){
   ComPtr<ID3D11Texture2D> t;if(FAILED(resource->QueryInterface(IID_PPV_ARGS(&t))))return false;
   D3D11_TEXTURE2D_DESC d={};t->GetDesc(&d);
   if(d.Width!=1024||d.Height!=1024||d.ArraySize!=1||d.SampleDesc.Count!=1||
      (d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM)){
    vsFault=true;Logf("sand surface WARN deposit texture layout unsupported (format=%u); native terrain retained",unsigned(d.Format));return false;}
   vsMapView.Reset();if(FAILED(device->CreateShaderResourceView(resource,nullptr,&vsMapView)))return false;vsMapResource=resource;
  }
  ID3D11ShaderResourceView*oldViews[VSand::ResourceCount]={};ID3D11Buffer*oldBuffer=nullptr;ID3D11SamplerState*oldSampler=nullptr;
  vsContext->PSGetShaderResources(VSand::MapSlot,VSand::ResourceCount,oldViews);for(unsigned i=0;i<VSand::ResourceCount;++i)views[i].Attach(oldViews[i]);
  vsContext->PSGetConstantBuffers(VSand::BufferSlot,1,&oldBuffer);buffer.Attach(oldBuffer);
  vsContext->PSGetSamplers(VSand::SamplerSlot,1,&oldSampler);sampler.Attach(oldSampler);
  // No other renderer/mod may own the reserved slots. Never overwrite it.
  bool occupied=oldBuffer||oldSampler;for(auto*v:oldViews)occupied|=v!=nullptr;
  if(occupied){vsFault=true;Logf("sand surface WARN reserved D3D slots already occupied; native terrain retained");return false;}
  // 0.4.1: an entry whose pair failed to load falls back to native.
  for(int i=0;i<2;++i)if(kinds[i]>0&&!vsTileReady[kinds[i]-1])kinds[i]=0;
  flags[0]=kinds[0]?1.f:0.f;flags[1]=kinds[1]?1.f:0.f;if(!flags[0]&&!flags[1])return false;
  float data[8]={};data[g_dep[vsDeposit].component]=vsStrength;data[4]=flags[0];data[5]=flags[1];
  memcpy(data+6,&VSand::Tag0,4);memcpy(data+7,&VSand::Tag1,4);
  vsContext->UpdateSubresource(vsBuffer.Get(),0,nullptr,data,0,0);
  int fallback=0;for(int t=0;t<g_tileCount;++t)if(vsTileReady[t]){fallback=t;break;}
  int t0=kinds[0]?kinds[0]-1:fallback,t1=kinds[1]?kinds[1]-1:fallback;
  ID3D11ShaderResourceView*now[]={vsMapView.Get(),vsTileColor[t0].Get(),vsTileNormal[t0].Get(),vsTileColor[t1].Get(),vsTileNormal[t1].Get()};auto*cb=vsBuffer.Get();auto*ss=vsSampler.Get();
  vsContext->PSSetShaderResources(VSand::MapSlot,VSand::ResourceCount,now);vsContext->PSSetConstantBuffers(VSand::BufferSlot,1,&cb);vsContext->PSSetSamplers(VSand::SamplerSlot,1,&ss);bound=true;
  if(!vsLogged){vsLogged=true;Logf("sand surface active: token=%s map=%d component=%d strength=%.2f; read-only GPU mask, native snow/lights retained",g_dep[vsDeposit].token,g_dep[vsDeposit].map,g_dep[vsDeposit].component,vsStrength);}
  return true;
 }
 void End(){if(!bound)return;bound=false;float zero[8]={};vsContext->UpdateSubresource(vsBuffer.Get(),0,nullptr,zero,0,0);
  ID3D11ShaderResourceView*old[VSand::ResourceCount];for(unsigned i=0;i<VSand::ResourceCount;++i)old[i]=views[i].Get();auto*cb=buffer.Get();auto*ss=sampler.Get();
  vsContext->PSSetShaderResources(VSand::MapSlot,VSand::ResourceCount,old);vsContext->PSSetConstantBuffers(VSand::BufferSlot,1,&cb);vsContext->PSSetSamplers(VSand::SamplerSlot,1,&ss);
 }
 ~VsBinding(){End();}
};
static bool VsBeginGuard(VsBinding*b,void*self,bool materials,int season,int transition){
 __try{return b->Begin(self,materials,season,transition);}
 __except(FaultFilter("sand surface binding",GetExceptionInformation())){vsFault=true;return false;}
}
static void h_VsRender(void*self,bool a,void*c1,bool tessellation,int b,int c){
 VsBinding binding;
 try{VsBeginGuard(&binding,self,a,b,c);}catch(...){vsFault=true;Logf("sand surface WARN resource allocation failed; native terrain retained");}
 vsOriginalRender(self,a,c1,tessellation,b,c);
}
static bool VsInstall(){
 const char*ini="plugins\\deposits_plus.ini";char setting[96];double n;
 H->configString(ini,"deposits_plus","sand_surface",setting,sizeof(setting),"0");
 if(!GenerationNumber(setting,0,1,&n)||n!=floor(n)){Logf("sand surface WARN invalid sand_surface value; disabled");return false;}
 if(!n)return false;
 H->configString(ini,"deposits_plus","sand_surface_strength",setting,sizeof(setting),"1.0");
 if(!GenerationNumber(setting,0,1,&n)){Logf("sand surface WARN invalid strength (expected 0..1); disabled");return false;}vsStrength=(float)n;
 H->configString(ini,"deposits_plus","sand_surface_token",setting,sizeof(setting),"$TYPE_MINE_SAND");
 for(int i=0;i<g_depCount;++i)if(!_stricmp(g_dep[i].token,setting))vsDeposit=i;
 // 0.4.1: tile table; an INI without [sand_tile:] sections keeps the classic meadow pair.
 if(!g_tileCount){
  static const struct{const char*id,*base,*color,*normal;} classic[]={{"meadow","tiles_normal/grass2.dds","sand_meadow_color.dds","sand_meadow_normal.dds"},{"meadow_autumn","tiles_normal/grass2_fall.dds","sand_meadow_autumn_color.dds","sand_meadow_autumn_normal.dds"}};
  for(const auto&c:classic){SandTile&t=g_tiles[g_tileCount++];strcpy_s(t.id,c.id);strcpy_s(t.base,c.base);strcpy_s(t.color,c.color);strcpy_s(t.normal,c.normal);}
  Logf("sand surface tiles: no [sand_tile:] section, classic meadow summer/autumn pair assumed");
 }
 int kept=0;
 for(int t=0;t<g_tileCount;++t){
  SandTile&x=g_tiles[t];
  // 0.4.2: color/normal are paths relative to the assets folder (set folders like "Siberia/x.dds"
  // allowed); climbing out, drive letters and absolute paths are refused.
  if(!x.base[0]||!x.color[0]||!x.normal[0]||!VsAssetPathOk(x.color)||!VsAssetPathOk(x.normal)){Logf("sand surface WARN [sand_tile:%s] needs base, color and normal (files relative to deposits_plus\\assets, no '..', no absolute paths); ignored",x.id);continue;}
  if(kept!=t)g_tiles[kept]=x;++kept;
 }
 g_tileCount=kept;
 for(int t=0;t<g_tileCount;++t)Logf("sand surface tile %s: base=%s color=%s normal=%s",g_tiles[t].id,g_tiles[t].base,g_tiles[t].color,g_tiles[t].normal);
 if(!g_tileCount){Logf("sand surface WARN no valid [sand_tile:] entry; disabled");return false;}
 if(vsDeposit<0||g_dep[vsDeposit].map<DEP_MAP_EXTRA||g_dep[vsDeposit].map>=MAX_MAPS){Logf("sand surface WARN token %s has no independent map; disabled",setting);return false;}
 auto*dos=(IMAGE_DOS_HEADER*)g_engine;
 if(!ReadablePtr(dos,sizeof(IMAGE_DOS_HEADER))||dos->e_magic!=IMAGE_DOS_SIGNATURE){Logf("sand surface WARN engine module has no readable DOS header; disabled");return false;}
 auto*nt=(IMAGE_NT_HEADERS*)((BYTE*)g_engine+dos->e_lfanew);
 if(!ReadablePtr(nt,sizeof(IMAGE_NT_HEADERS))||nt->Signature!=IMAGE_NT_SIGNATURE){Logf("sand surface WARN engine module has no readable PE header; disabled");return false;}
 if(nt->FileHeader.TimeDateStamp!=0x6A3E75BCu||nt->OptionalHeader.SizeOfImage!=0x1F2000u){Logf("sand surface WARN unsupported engine build; disabled");return false;}
 auto*create=(BYTE*)GetProcAddress(g_engine,"?CreateShaders@C3DAPI_D3D11_SHADERS@@UEAAHPEBD@Z");
 const BYTE expected[]={0x4c,0x8b,0x0d,0x3f,0x07,0x1c,0x00};
 if(!create||!ReadablePtr(create+0x2ba,7)||memcmp(create+0x2ba,expected,7)){Logf("sand surface WARN D3D device accessor signature differs; disabled");return false;}
 vsDeviceSlot=(void**)(create+0x2c1+*(int32_t*)(create+0x2bd));
 vsTexture=(VsGetTexture)GetProcAddress(g_engine,"?TextureGet@C3D_MATERIAL@@QEAAPEAVC3DAPI_TEXTURE@@I@Z");
 vsTextureName=(VsTextureName)GetProcAddress(g_engine,"?GetTextureFileName@C3DAPI_TEXTURE@@QEAAPEADXZ");
 vsResource=(VsTextureResource)GetProcAddress(g_engine,"?GetTextureResource@C3DAPI_D3D11_TEXTURE@@QEAAPEAUID3D11Resource@@XZ");
 const char*materialNames[]={"?GetMaterial@C3D_TERRAIN@@QEAAPEAVC3D_MATERIAL@@XZ","?GetMaterialFall@C3D_TERRAIN@@QEAAPEAVC3D_MATERIAL@@XZ","?GetMaterialWinter@C3D_TERRAIN@@QEAAPEAVC3D_MATERIAL@@XZ"};
 for(int i=0;i<3;++i)vsMaterials[i]=(VsGetMaterial)GetProcAddress(g_engine,materialNames[i]);
 // CreateManagedShaders only allocates/registers an EMPTY object. The game
 // later invokes CreateShaders through vtable slot 0. Hook that actual load,
 // never infer completion from the earlier placeholder-registration call.
 auto**shaderTable=(void**)GetProcAddress(g_engine,"??_7C3DAPI_D3D11_SHADERS@@6B@");
 if(!ReadablePtr(shaderTable,sizeof(void*))||shaderTable[0]!=(void*)create){
  Logf("sand surface WARN native shader load slot differs or is already hooked; disabled");return false;}
 const char*render="?Render@C3D_TERRAIN@@QEAAX_NPEAVC3D_CAMERA@@0HH@Z";
 if(!vsTexture||!vsTextureName||!vsResource||!vsMaterials[0]||!vsMaterials[1]||!vsMaterials[2]||!FindIatSlot(g_exe,DLL_ENGINE,render)){
  Logf("sand surface WARN required engine interface unavailable; disabled");return false;}
 vsEnabled=true;
 vsOriginalLoad=(VsLoad)create;
 if(!VsSwap(shaderTable,(void*)create,(void*)h_VsLoad)){
  vsEnabled=false;Logf("sand surface WARN cannot attach actual shader load; disabled");return false;}
 if(!PatchIat(g_exe,DLL_ENGINE,render,(void*)h_VsRender,(void**)&vsOriginalRender,"sand surface render")){
  vsEnabled=false;if(!VsSwap(shaderTable,(void*)h_VsLoad,(void*)create))Logf("sand surface WARN shader load rollback conflict; inactive pass-through retained");return false;}
 Logf("sand surface prepared: actual CreateShaders load hook, token=%s, strength=%.2f, native textures/maps/saves unchanged",g_dep[vsDeposit].token,vsStrength);return true;
}
}
