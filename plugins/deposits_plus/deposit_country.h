// Pure CPU snapshot validation/rasterization of the game's BORDER_POLYGON.
// Vertices are in world X/Z coordinates, as loaded by C3D_TERRAIN. No game writes.
#pragma once
#include "deposit_generation.h"

namespace deposit_generation {
struct BorderVertex { float x,y,z; };
inline double Cross(const BorderVertex& a,const BorderVertex& b,const BorderVertex& c) {
    return (double(b.x)-a.x)*(double(c.z)-a.z)-(double(b.z)-a.z)*(double(c.x)-a.x);
}
inline bool OnSegment(const BorderVertex& a,const BorderVertex& b,const BorderVertex& c) {
    return Cross(a,b,c)==0 && c.x>=(std::min)(a.x,b.x) && c.x<=(std::max)(a.x,b.x) &&
                            c.z>=(std::min)(a.z,b.z) && c.z<=(std::max)(a.z,b.z);
}
inline bool SegmentsTouch(const BorderVertex& a,const BorderVertex& b,const BorderVertex& c,const BorderVertex& d) {
    double ac=Cross(a,b,c),ad=Cross(a,b,d),ca=Cross(c,d,a),cb=Cross(c,d,b);
    return (((ac>0 && ad<0)||(ac<0 && ad>0)) && ((ca>0 && cb<0)||(ca<0 && cb>0))) ||
        OnSegment(a,b,c) || OnSegment(a,b,d) || OnSegment(c,d,a) || OnSegment(c,d,b);
}
inline bool ValidateBorder(std::vector<BorderVertex>& vertices,std::string& error) {
    if(vertices.size()<3 || vertices.size()>4096) { error="border requires 3..4096 vertices"; return false; }
    std::vector<BorderVertex> clean;
    for(const auto& p:vertices) {
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) { error="non-finite border vertex"; return false; }
        if(clean.empty() || clean.back().x!=p.x || clean.back().z!=p.z) clean.push_back(p);
    }
    if(clean.size()>1 && clean.front().x==clean.back().x && clean.front().z==clean.back().z) clean.pop_back();
    if(clean.size()<3) { error="border has fewer than three distinct vertices"; return false; }
    double area=0;
    for(size_t i=0;i<clean.size();++i) {
        size_t next=(i+1)%clean.size();
        area+=double(clean[i].x)*clean[next].z-double(clean[i].z)*clean[next].x;
        for(size_t j=i+1;j<clean.size();++j) {
            size_t jnext=(j+1)%clean.size();
            if(j==next || jnext==i) continue;
            if(SegmentsTouch(clean[i],clean[next],clean[j],clean[jnext])) {
                error="self-intersecting or touching border polygon"; return false;
            }
        }
    }
    if(fabs(area)<1) { error="border polygon has no usable area"; return false; }
    vertices=std::move(clean); return true;
}
inline bool InsideBorder(const std::vector<BorderVertex>& poly,double x,double z) {
    bool inside=false;
    for(size_t i=0,j=poly.size()-1;i<poly.size();j=i++) {
        const auto& a=poly[i]; const auto& b=poly[j];
        if((a.z>z)!=(b.z>z) && x<(double(b.x)-a.x)*(z-a.z)/(double(b.z)-a.z)+a.x) inside=!inside;
    }
    return inside;
}
inline bool ClipLine(double& x0,double& y0,double& x1,double& y1) {
    double dx=x1-x0,dy=y1-y0,t0=0,t1=1;
    double p[]={-dx,dx,-dy,dy},q[]={x0,double(Side)-x0,y0,double(Side)-y0};
    for(unsigned i=0;i<4;++i) {
        if(p[i]==0) { if(q[i]<0) return false; continue; }
        double t=q[i]/p[i];
        if(p[i]<0) t0=(std::max)(t0,t); else t1=(std::min)(t1,t);
        if(t0>t1) return false;
    }
    x1=x0+t1*dx; y1=y0+t1*dy; x0+=t0*dx; y0+=t0*dy; return true;
}
inline Bytes CountryMask(const std::vector<BorderVertex>& poly,double ox,double oz,double sx,double sz) {
    if(poly.size()<3 || !std::isfinite(ox)||!std::isfinite(oz)||!std::isfinite(sx)||
       !std::isfinite(sz)||sx<=0||sz<=0) return {};
    Bytes outside(Cells,1); std::vector<double> crossings;
    // Odd/even crossing rule, also used by the game's building-border check.
    for(unsigned y=0;y<Side;++y) {
        crossings.clear(); double z=oz+(y+.5)*sz/Side;
        for(size_t i=0,j=poly.size()-1;i<poly.size();j=i++) {
            const auto& a=poly[i]; const auto& b=poly[j];
            if((a.z>z)!=(b.z>z)) crossings.push_back((double(b.x)-a.x)*(z-a.z)/(double(b.z)-a.z)+a.x);
        }
        std::sort(crossings.begin(),crossings.end());
        for(size_t i=1;i<crossings.size();i+=2) {
            double left=(crossings[i-1]-ox)*Side/sx-.5,right=(crossings[i]-ox)*Side/sx-.5;
            int begin=(int)ceil((std::max)(0.,(std::min)(double(Side),left)));
            int end=(int)ceil((std::max)(0.,(std::min)(double(Side),right)));
            for(int x=begin;x<end;++x) outside[y*Side+x]=0;
        }
    }
    // Exclude cells along the actual boundary as well. Half-cell sampling plus
    // its immediate neighbours conservatively covers diagonals/concave corners:
    // a retained texel lies wholly inside, not merely its centre.
    for(size_t i=0,j=poly.size()-1;i<poly.size();j=i++) {
        double x0=(poly[j].x-ox)*Side/sx,y0=(poly[j].z-oz)*Side/sz;
        double x1=(poly[i].x-ox)*Side/sx,y1=(poly[i].z-oz)*Side/sz;
        if(!ClipLine(x0,y0,x1,y1)) continue;
        unsigned steps=(unsigned)ceil(2*(std::max)(fabs(x1-x0),fabs(y1-y0)))+1;
        for(unsigned k=0;k<=steps;++k) {
            int x=(int)floor(x0+(x1-x0)*k/steps),y=(int)floor(y0+(y1-y0)*k/steps);
            for(int yy=(std::max)(0,y-1);yy<=(std::min)(int(Side)-1,y+1);++yy)
                for(int xx=(std::max)(0,x-1);xx<=(std::min)(int(Side)-1,x+1);++xx) outside[yy*Side+xx]=1;
        }
    }
    return outside;
}
} // namespace deposit_generation
