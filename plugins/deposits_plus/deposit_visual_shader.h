#pragma once
// Add an independent surface mask at the native grass SAMPLE sites, before
// native terrain blending, snow, lights and fog. Never replace an entire FX.
// Fail closed on layouts outside the small DXBC subset verified by the tests.
#include <windows.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include "third_party/d3d12TokenizedProgramFormat.hpp"
namespace DepositVisualHash {
#include "third_party/DxilHash.cpp"
}
namespace DepositVisualShader {
using Microsoft::WRL::ComPtr;
using Words=std::vector<uint32_t>;
using Bytes=std::vector<uint8_t>;
constexpr unsigned MapSlot=120, ColorSlot=121, NormalSlot=122, BufferSlot=13, SamplerSlot=14;
constexpr unsigned SecondColorSlot=123, SecondNormalSlot=124, ResourceCount=5;
constexpr uint32_t Tag0=0x544d5344,Tag1=0x32524653;
struct Result { Bytes bytes; unsigned sites=0; std::string reason; };
inline uint32_t Read(const uint8_t*p) {uint32_t v;memcpy(&v,p,4);return v;}
inline void Write(Bytes&b,size_t p,uint32_t v) {memcpy(b.data()+p,&v,4);}
inline uint32_t Op(unsigned op,unsigned n,unsigned flags=0){return op|(n<<24)|flags;}
inline uint32_t Dst(unsigned mask=15){return 2|(mask<<4)|(1<<20);}
inline uint32_t Src(unsigned type=0,unsigned swizzle=0xe4){return 2|4|(swizzle<<4)|(type<<12)|(1<<20);}
inline uint32_t Cb(unsigned swizzle=0xe4){return 2|4|(swizzle<<4)|(8<<12)|(2<<20);}
inline uint32_t Reg(unsigned type){return (type<<12)|(1<<20);}
inline void Add(Words&w,std::initializer_list<uint32_t>v){w.insert(w.end(),v);}
inline bool Operand(const Words&w,size_t start,size_t end,size_t&next,unsigned depth=0) {
 if(start>=end||depth>8)return false;
 uint32_t t=w[start]; size_t p=start+1; uint32_t ext=t;
 while(ext&0x80000000u){if(p>=end)return false;ext=w[p++];}
 unsigned type=(t>>12)&255,dim=(t>>20)&3,comp=t&3;
 if(type==4||type==5){if(dim||comp==0||comp==3)return false;p+=(comp==1?1:4)*(type==5?2:1);}
 else for(unsigned d=0;d<dim;++d){unsigned rep=(t>>(22+3*d))&7;if(rep>4)return false;
  if(rep==0||rep==3)++p;else if(rep==1||rep==4)p+=2;
  if(p>end)return false;
  if(rep>=2){size_t n;if(!Operand(w,p,end,n,depth+1))return false;p=n;}
 }
 if(p>end)return false;next=p;return true;
}
inline bool Simple(const Words&w,size_t p,size_t n,unsigned type){
 return n==p+2 && ((w[p]>>12)&255)==type && ((w[p]>>20)&3)==1 && !(w[p]&0xffc00000u);
}
inline bool Patch(const void*data,size_t length,Result&out) {
 out=Result();
 auto fail=[&](const char*why){out.reason=why;out.bytes.clear();return false;};
 if(!data||length<32||length>2*1024*1024)return fail("invalid DXBC length");
 const auto*b=(const uint8_t*)data;
 if(memcmp(b,"DXBC",4)||Read(b+24)!=length)return fail("invalid DXBC header");
 uint8_t hash[16];DepositVisualHash::ComputeHashRetail(b+20,(UINT)length-20,hash);
 if(memcmp(hash,b+4,16))return fail("DXBC checksum mismatch");
 ComPtr<ID3D11ShaderReflection> reflection;
 if(FAILED(D3DReflect(data,length,IID_PPV_ARGS(&reflection))))return fail("reflection unavailable");
 D3D11_SHADER_DESC desc={};if(FAILED(reflection->GetDesc(&desc)))return fail("reflection invalid");
 if(D3D11_SHVER_GET_TYPE(desc.Version)!=D3D11_SHVER_PIXEL_SHADER||desc.InputParameters<6)
  return fail("not a terrain surface pixel shader");
 int slots[4]={-1,-1,-1,-1};int coord=-1;
 bool mask=false;
 for(UINT i=0;i<desc.BoundResources;++i){D3D11_SHADER_INPUT_BIND_DESC d={};if(FAILED(reflection->GetResourceBindingDesc(i,&d)))return fail("resource reflection invalid");
  if((d.Type==D3D_SIT_TEXTURE&&d.BindPoint+d.BindCount>MapSlot)||
     (d.Type==D3D_SIT_CBUFFER&&d.BindPoint+d.BindCount>BufferSlot)||
     (d.Type==D3D_SIT_SAMPLER&&d.BindPoint+d.BindCount>SamplerSlot))return fail("reserved visual slots already in use");
  const char*names[]={"Texture2DStage5","Texture2DStage6","Texture2DStage25","Texture2DStage26"};
  for(int j=0;j<4;++j)if(!strcmp(d.Name,names[j])&&d.Type==D3D_SIT_TEXTURE&&d.Dimension==D3D_SRV_DIMENSION_TEXTURE2D&&d.BindCount==1)slots[j]=(int)d.BindPoint;
  if(!strcmp(d.Name,"Texture2DStage2"))mask=true;
 }
 if(!mask||slots[0]<0)return fail("no native terrain grass layer");
 for(UINT i=0;i<desc.InputParameters;++i){D3D11_SIGNATURE_PARAMETER_DESC d={};reflection->GetInputParameterDesc(i,&d);
  if(!_stricmp(d.SemanticName,"TEXCOORD")&&d.SemanticIndex==0&&(d.Mask&3)==3)coord=(int)d.Register;
 }
 if(coord<0)return fail("world UV input missing");
 unsigned chunks=Read(b+28);if(chunks>64||32+4*chunks>length)return fail("invalid chunk table");
 size_t codeOffset=0,codeBytes=0;unsigned codeIndex=0;
 for(unsigned i=0;i<chunks;++i){size_t p=Read(b+32+4*i);if(p>length-8)return fail("invalid chunk offset");
  size_t n=Read(b+p+4);if(n>length-p-8)return fail("invalid chunk length");
  if(!memcmp(b+p,"SHDR",4)||!memcmp(b+p,"SHEX",4)){if(codeOffset)return fail("multiple programs");codeOffset=p;codeBytes=n;codeIndex=i;}
 }
 if(!codeOffset||codeBytes<8||codeBytes%4)return fail("program missing");
 Words code(codeBytes/4);memcpy(code.data(),b+codeOffset+8,codeBytes);
 if(code[1]!=code.size()||(code[0]!=0x40&&code[0]!=0x50))return fail("unsupported shader version/length");
 unsigned temp=0;size_t tempPos=0,body=2;
 for(size_t p=2;p<code.size();){unsigned op=code[p]&0x7ff,n=(code[p]>>24)&127;
  if(op==D3D10_SB_OPCODE_CUSTOMDATA){if(p+1>=code.size())return fail("invalid custom block");n=code[p+1];}
  if(!n||n>code.size()-p)return fail("invalid instruction length");
  if(op==D3D10_SB_OPCODE_DCL_TEMPS){if(n!=2||tempPos)return fail("unsupported temporaries");temp=code[p+1];tempPos=p;}
  if(op>=D3D10_SB_OPCODE_DCL_RESOURCE&&op<=D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS)body=p+n;
  else if(op!=D3D10_SB_OPCODE_CUSTOMDATA)break;
  p+=n;
 }
 if(!tempPos||temp>120)return fail("temporaries unavailable");
 Words patched(code.begin(),code.begin()+body);patched[tempPos+1]+=3;
 for(unsigned r=MapSlot;r<=SecondNormalSlot;++r)Add(patched,{Op(D3D10_SB_OPCODE_DCL_RESOURCE,4,3<<11),Reg(7),r,0x5555});
 Add(patched,{Op(D3D10_SB_OPCODE_DCL_SAMPLER,3),Reg(6),SamplerSlot});
 Add(patched,{Op(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER,4),Cb(),BufferSlot,2});
 // x = richness, y = site weight, z = valid owner tag. A foreign constant
 // buffer in b13 must never activate this shader, even with nonzero/NaN flags.
 Add(patched,{Op(D3D10_SB_OPCODE_MOV,5),Dst(),temp+2,0x4001,0});
 Add(patched,{Op(D3D10_SB_OPCODE_IEQ,8),Dst(4),temp+2,Cb(0xaa),BufferSlot,1,0x4001,Tag0});
 Add(patched,{Op(D3D10_SB_OPCODE_IEQ,8),Dst(8),temp+2,Cb(0xff),BufferSlot,1,0x4001,Tag1});
 Add(patched,{Op(D3D10_SB_OPCODE_AND,7),Dst(4),temp+2,Src(0,0xaa),temp+2,Src(0,0xff),temp+2});
 Add(patched,{Op(D3D10_SB_OPCODE_IF,3,1<<18),Src(0,0xaa),temp+2});
 Add(patched,{Op(D3D10_SB_OPCODE_SAMPLE_L,11),Dst(),temp+1,Src(1,0x04),(unsigned)coord,Src(7),MapSlot,Reg(6),SamplerSlot,0x4001,0});
 Add(patched,{Op(D3D10_SB_OPCODE_DP4,8,1<<13),Dst(1),temp+2,Src(),temp+1,Cb(),BufferSlot,0});
 Add(patched,{Op(D3D10_SB_OPCODE_ENDIF,1)});
 for(size_t p=body;p<code.size();){unsigned op=code[p]&0x7ff,n=(code[p]>>24)&127;
  if(op==D3D10_SB_OPCODE_CUSTOMDATA){if(p+1>=code.size())return fail("truncated custom block");n=code[p+1];}
  if(!n||n>code.size()-p)return fail("bad instruction");
  bool sample=op==D3D10_SB_OPCODE_SAMPLE||op==D3D10_SB_OPCODE_SAMPLE_L||op==D3D10_SB_OPCODE_SAMPLE_D||op==D3D10_SB_OPCODE_SAMPLE_B;
  int group=-1;size_t dst=0,dstEnd=0,uvEnd=0,resEnd=0;
  if(sample){size_t s=p+1;uint32_t extended=code[p];while(extended&0x80000000u){if(s>=p+n)return fail("bad sample extension");extended=code[s++];}dst=s;
   if(!Operand(code,s,p+n,dstEnd)||!Operand(code,dstEnd,p+n,uvEnd)||!Operand(code,uvEnd,p+n,resEnd))return fail("bad sample operands");
   if(Simple(code,uvEnd,resEnd,7))for(int j=0;j<4;++j)if(slots[j]>=0&&code[uvEnd+1]==(unsigned)slots[j])group=j;
  }
  if(group>=0 && (code[dst]&0x70)) {
   if(!Simple(code,dst,dstEnd,0)||(code[dst]&15)!=2)return fail("unsupported sample destination");
   unsigned writeMask=(code[dst]>>4)&7;unsigned originalReg=code[dst+1];
   unsigned swizzle=group>=2?0x55:0;
   Add(patched,{Op(D3D10_SB_OPCODE_MOVC,10),Dst(2),temp+2,Src(0,0xaa),temp+2,Cb(swizzle),BufferSlot,1,0x4001,0});
   // Texture derivatives must stay valid at the deposit boundary: sample in
   // uniform material/owner control flow, not in the varying richness branch.
   Add(patched,{Op(D3D10_SB_OPCODE_IF,3,1<<18),Src(0,0x55),temp+2});
   // Each native seasonal layer needs its own colour/normal pair. Otherwise
   // a summer->autumn crossfade would apply the green tile to both layers.
   Words clone(code.begin()+p,code.begin()+p+n);clone[dst-p+1]=temp;
   clone[uvEnd-p+1]=group>=2?((group&1)?SecondNormalSlot:SecondColorSlot):((group&1)?NormalSlot:ColorSlot);
   patched.insert(patched.end(),clone.begin(),clone.end());
   Add(patched,{Op(D3D10_SB_OPCODE_ENDIF,1)});
   Add(patched,{Op(D3D10_SB_OPCODE_MUL,7),Dst(2),temp+2,Src(0,0),temp+2,Src(0,0x55),temp+2});
   // Sample the native texture AFTER our clone: its destination may also be
   // its input coordinate, so the sources must still have their original values.
   patched.insert(patched.end(),code.begin()+p,code.begin()+p+n);
   Add(patched,{Op(D3D10_SB_OPCODE_IF,3,1<<18),Src(0,0x55),temp+2});
   Add(patched,{Op(D3D10_SB_OPCODE_ADD,8),Dst(writeMask),temp+1,Src(),temp,Src()|0x80000000u,0x41,originalReg});
   Add(patched,{Op(D3D10_SB_OPCODE_MAD,9),Dst(writeMask),originalReg,Src(0,0x55),temp+2,Src(),temp+1,Src(),originalReg});
   Add(patched,{Op(D3D10_SB_OPCODE_ENDIF,1)});++out.sites;
  }else patched.insert(patched.end(),code.begin()+p,code.begin()+p+n);
  p+=n;
 }
 if(!out.sites)return fail("no compatible grass samples");
 patched[1]=(uint32_t)patched.size();
 // Rebuild offsets rather than assuming SHDR is the final chunk. Reflection
 // deliberately stays native: the FX binder must not own our reserved slots.
 out.bytes.assign(b,b+32+4*chunks);
 for(unsigned i=0;i<chunks;++i){while(out.bytes.size()%4)out.bytes.push_back(0);size_t p=Read(b+32+4*i);Write(out.bytes,32+4*i,(uint32_t)out.bytes.size());
  size_t old=out.bytes.size();
  if(i==codeIndex){out.bytes.insert(out.bytes.end(),b+p,b+p+8);Write(out.bytes,old+4,(uint32_t)patched.size()*4);auto*q=(const uint8_t*)patched.data();out.bytes.insert(out.bytes.end(),q,q+patched.size()*4);}
  else out.bytes.insert(out.bytes.end(),b+p,b+p+8+Read(b+p+4));
 }
 Write(out.bytes,24,(uint32_t)out.bytes.size());DepositVisualHash::ComputeHashRetail(out.bytes.data()+20,(UINT)out.bytes.size()-20,out.bytes.data()+4);
 out.reason="native grass samples augmented; original alpha and all other instructions preserved";return true;
}
}
