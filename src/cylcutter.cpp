/*  Copyright 2010 Anders Wallin (anders.e.e.wallin "at" gmail.com)
 *  
 *  This file is part of OpenCAMlib.
 *
 *  OpenCAMlib is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenCAMlib is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenCAMlib.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <math.h>
#include <cassert>

#include "cutter.h"
#include "point.h"
#include "triangle.h"
#include "cutter.h"
#include "numeric.h"

// #define EDGEDROP_DEBUG

//********   CylCutter ********************** */
CylCutter::CylCutter()
{
    setDiameter(1.0);
}

CylCutter::CylCutter(const double d)
{
    setDiameter(d);
}

//********   drop-cutter methods ********************** */
int CylCutter::vertexDrop(Point &cl, CCPoint &cc, const Triangle &t) const
{
    /// loop through each vertex p of Triangle t
    /// drop down cutter at (cl.x, cl.y) against Point p
    
    int result = 0;
    BOOST_FOREACH( const Point& p, t.p)
    {
        double q = cl.xyDistance(p); // distance in XY-plane from cl to p
        if (q<= diameter/2) { // p is inside the cutter
            if (cl.liftZ(p.z)) { // we need to lift the cutter
                cc = p;
                cc.type = VERTEX;
                result = 1;
            }
        } else {
            // point outside cutter, nothing to do.
        }
    }
    return result;
}

int CylCutter::facetDrop(Point &cl, CCPoint &cc, const Triangle &t) const
{
    // Drop cutter at (cl.x, cl.y) against facet of Triangle t
    
    //std::cout << "facetDrop triangle=" << t << "\n";
    //std::cout << "facetDrop input normal=" << *t.n << "\n";
    Point normal; // facet surface normal
    
    if ( isZero_tol( t.n->z ) )  { // vertical surface
        //std::cout << "facetDrop vertical case. bye.\n";
        return -1;  // can't drop against vertical surface
    } else if (t.n->z < 0) {  // normal is pointing down
        normal = -1* (*t.n); // flip normal
        //std::cout << "facetDrop flip normal\n";
    } else {
        //std::cout << "facetDrop normal case\n";
        normal = *t.n;
    }
    
    assert( isPositive( normal.z ) ); // we are in trouble if n.z is not positive by now...
    
    //std::cout << "facetDrop up-flipped normal="<<normal<<"\n";
    
    
    // define plane containing facet
    // a*x + b*y + c*z + d = 0, so
    // d = -a*x - b*y - c*z, where
    // (a,b,c) = surface normal
    double a = normal.x;
    double b = normal.y;
    double c = normal.z;
    //double d = - a * t.p[0].x - b * t.p[0].y - c * t.p[0].z;
    double d = - normal.dot(t.p[0]);
        
    normal.xyNormalize(); // make length of normal in xy plane == 1.0
    
    //std::cout << "xyNormalized : n="<<normal<<"\n";
    
    // the contact point with the plane is on the periphery
    // of the cutter, a length diameter/2 from cl in the direction of -n
    Point cc_tmp = cl - (diameter/2)*normal; // Note: at this point the z-coord is rubbish.
    
    if (cc_tmp.isInside(t)) { // NOTE: cc.z is ignored in isInside()
        cc_tmp.z = (1.0/c)*(-d-a*cc_tmp.x-b*cc_tmp.y); // NOTE: potential for divide-by-zero (?!)
        if (cl.liftZ(cc_tmp.z)) {
            cc = cc_tmp;
            cc.type = FACET;
            return 1;
        }
    } else {
        //std::cout << " NOT isInside!, cc="<<cc<<"\n";
        return 0;
    }
    return 0;
}


int CylCutter::edgeDrop(Point &cl, CCPoint &cc, const Triangle &t) const
{
    // Drop cutter at (p.x, p.y) against edges of Triangle t
    // strategy:
    // 1) calculate distance to infinite line
    // 2) calculate intersection points w. cutter
    // 3) pick the higher intersection point and test if it is in the edge
    //Point cc;
    int result = 0;
    
    for (int n=0;n<3;n++) { // loop through all three edges
    
        // 1) distance from point to line
        int start=n;
        int end=(n+1)%3;
        
        
        #ifdef EDGEDROP_DEBUG
            std::cout << "testing points " << start<< " to " << end << " :";
        #endif
        
        Point p1 = t.p[start];
        Point p2 = t.p[end];
        
        // check that there is an edge in the xy-plane
        // can't drop against vertical edges!
        if ( !isZero_tol( p1.x - p2.x ) || !isZero_tol( p1.y - p2.y) ) {
            
            double d = cl.xyDistanceToLine(t.p[start],t.p[end]);
            
            #ifdef EDGEDROP_DEBUG
                std::cout << "xyDistance=" << d ;
            #endif
            
            if (d<=diameter/2) { // potential hit
                //std::cout << " potential hit\n";
                // 2) calculate intersection points w. cutter circle
                // points are on line and diameter/2 from cl
                // see http://mathworld.wolfram.com/Circle-LineIntersection.html
                double x1 = t.p[start].x - cl.x; // translate to cl=(0,0)
                double y1 = t.p[start].y - cl.y;
                double x2 = t.p[end].x - cl.x;
                double y2 = t.p[end].y - cl.y;
                double dx = x2-x1;
                double dy = y2-y1;
                double dr = sqrt( dx*dx + dy*dy);
                double D = x1*y2 - x2*y1;
                double discr = square( diameter/2 ) * square(dr) - square(D);
                #ifdef EDGEDROP_DEBUG
                    std::cout << "discr=" << discr << "\n";
                #endif
                
                if ( !isZero_tol(discr) && isNegative(discr) ) {
                    std::cout << "cutter.cpp ERROR: CylCutter::edgeTest discr= "<<discr<<" <0 !!\n";
                    cc.type = ERROR;
                    assert(0);
                    return -1;
                    
                } else if ( isZero_tol(discr) ) {// tangent line
                    #ifdef EDGEDROP_DEBUG
                        std::cout << "discr= zero, tangent case.\n";
                    #endif
                    cc.x = D*dy / pow(dr,2) + cl.x; // translate back to cl
                    cc.y = -D*dx / pow(dr,2) + cl.y;
                    // 3) check if cc is in edge
                    if ( cc.isInsidePoints(t.p[start], t.p[end]) ) { 
                        // determine height of point. must be on line, so:
                        // std::cout << "tangent-case: isInside=true!\n";
                        // two point formula for line:
                        // z-z1 = ((z2-z1)/(x2-x1)) * (x - x1)
                        // z = z1 + ((z2-z1)/(x2-x1)) * (x-x1)
                        double z1 = t.p[start].z;
                        double z2 = t.p[end].z;
                        double x1 = t.p[start].x;
                        double x2 = t.p[end].x;
                        double y1 = t.p[start].y;
                        double y2 = t.p[end].y;
                        if (x1 != x2) 
                            cc.z = z1 + ((z2-z1)/(x2-x1)) * (cc.x-x1);
                        else if (y1 != y2) 
                            cc.z = z1 + ((z2-z1)/(y2-y1)) * (cc.y-y1);
                           
                        if (cl.liftZ(cc.z))
                            cc.type = EDGE;
                    }
                } else { // discr > 0, two intersection points
                    #ifdef EDGEDROP_DEBUG
                        std::cout << "discr>0, two intersections\n";
                    #endif
                    Point cc1;
                    Point cc2;
                    // remember to translate back to cl
                    cc1.x= (D*dy  + sign(dy)*dx*sqrt(discr)) / pow(dr,2) + cl.x; 
                    cc1.y= (-D*dx + fabs(dy)*sqrt(discr)   ) / pow(dr,2) + cl.y;
                    cc1.z=0;
                    cc2.x= (D*dy  - sign(dy)*dx*sqrt(discr)) / pow(dr,2) + cl.x;
                    cc2.y= (-D*dx - fabs(dy)*sqrt(discr)   ) / pow(dr,2) + cl.y;
                    cc2.z=0;
                    #ifdef EDGEDROP_DEBUG
                        std::cout << "cc1= " << cc1 << "\n";
                        std::cout << "cc2= " << cc2 << "\n";
                    #endif
                    // 3) check if in edge
                    double z1 = t.p[start].z;
                    double z2 = t.p[end].z;
                    double x1 = t.p[start].x;
                    double x2 = t.p[end].x;
                    double y1 = t.p[start].y;
                    double y2 = t.p[end].y;
                    if ( cc1.isInsidePoints(t.p[start], t.p[end]) ) {
                        #ifdef EDGEDROP_DEBUG
                            std::cout << "cc1 is in edge!\n";
                        #endif
                        // determine height of point. must be on line, so:
                        if ( !isZero_tol( fabs(x1 - x2) )  ) {  // can compute using x-coords
                            #ifdef EDGEDROP_DEBUG
                                std::cout << "computing cc1 height from x-coords\n";
                            #endif
                            cc1.z = z1 + ((z2-z1)/(x2-x1)) * (cc1.x-x1);
                        }
                        else if ( !isZero_tol( fabs(y1 - y2) ) ) {
                            #ifdef EDGEDROP_DEBUG
                                std::cout << "computing cc1 height from y-coords\n";
                            #endif
                            cc1.z = z1 + ((z2-z1)/(y2-y1)) * (cc1.y-y1);
                        }
                        else {// we are in trouble.
                            std::cout << "cyclutter edge-test, unable to compute cc-point. stop.\n";
                            assert(0);
                        }
                        
                        if (cl.liftZ(cc1.z)) {
                            cc=cc1;
                            cc.type = EDGE;
                        }
                        //std::cout << "intersect case: cc1 isInside=true! cc1=" << cc1 << "\n";
                    }
                    if ( cc2.isInsidePoints(t.p[start], t.p[end]) ) {
                        #ifdef EDGEDROP_DEBUG
                            std::cout << "cc2 is in edge!\n";
                        #endif
                        // determine height of point. must be on line, so:
                        if ( !isZero_tol( fabs(x1 - x2) ) ) {
                            #ifdef EDGEDROP_DEBUG
                                std::cout << "computing cc2 height from x-coords\n";
                            #endif
                            cc2.z = z1 + ((z2-z1)/(x2-x1)) * (cc2.x-x1);
                        }
                        else if ( !isZero_tol( fabs(y1 - y2) )  ) {
                            #ifdef EDGEDROP_DEBUG
                                std::cout << "computing cc2 height from y-coords\n";
                            #endif
                            cc2.z = z1 + ((z2-z1)/(y2-y1)) * (cc2.y-y1);
                        }
                        else {// we are in trouble.
                            std::cout << "cyclutter edge-test, unable to compute cc-point. stop.\n";
                            assert(0);
                        }
                            
                        if (cl.liftZ(cc2.z)) {
                            cc = cc2;
                            cc.type = EDGE;
                        }
                        //std::cout << "intersect case: cc2 isInside=true! cc2=" << cc2 << "\n";
                    }
                    
                    
                } //end two intersection points case
                
            }// end if(potential hit)
            else {
                // edge is too far away from cutter. nothing to do.
            }
        } // end if(vertical edge)
        
    } // end loop through all edges
        
    return result;
}




//********  CylCutter string output ********************** */
std::string CylCutter::str()
{
    std::ostringstream o;
    o << "CylCutter"<< id <<"(d=" << diameter << ")";
    return o.str();
}

std::ostream& operator<<(std::ostream &stream, CylCutter c)
{
  stream << "CylCutter"<< c.id <<"(d=" << c.diameter << ")";
  return stream;
}

