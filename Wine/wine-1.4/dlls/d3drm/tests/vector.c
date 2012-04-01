/*
 * Copyright 2007 Vijay Kiran Kamuju
 * Copyright 2007 David Adam
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <math.h>

#include "d3drmdef.h"

#include "wine/test.h"

#define PI (4.0f*atanf(1.0f))
#define admit_error 0.000001f

#define expect_mat( expectedmat, gotmat)\
{ \
    int i,j,equal=1; \
    for (i=0; i<4; i++)\
        {\
         for (j=0; j<4; j++)\
             {\
              if (fabs(expectedmat[i][j]-gotmat[i][j])>admit_error)\
                 {\
                  equal=0;\
                 }\
             }\
        }\
    ok(equal, "Expected matrix=\n(%f,%f,%f,%f\n %f,%f,%f,%f\n %f,%f,%f,%f\n %f,%f,%f,%f\n)\n\n" \
       "Got matrix=\n(%f,%f,%f,%f\n %f,%f,%f,%f\n %f,%f,%f,%f\n %f,%f,%f,%f)\n", \
       expectedmat[0][0],expectedmat[0][1],expectedmat[0][2],expectedmat[0][3], \
       expectedmat[1][0],expectedmat[1][1],expectedmat[1][2],expectedmat[1][3], \
       expectedmat[2][0],expectedmat[2][1],expectedmat[2][2],expectedmat[2][3], \
       expectedmat[3][0],expectedmat[3][1],expectedmat[3][2],expectedmat[3][3], \
       gotmat[0][0],gotmat[0][1],gotmat[0][2],gotmat[0][3], \
       gotmat[1][0],gotmat[1][1],gotmat[1][2],gotmat[1][3], \
       gotmat[2][0],gotmat[2][1],gotmat[2][2],gotmat[2][3], \
       gotmat[3][0],gotmat[3][1],gotmat[3][2],gotmat[3][3] ); \
}

#define expect_quat(expectedquat,gotquat) \
  ok( (fabs(U1(expectedquat.v).x-U1(gotquat.v).x)<admit_error) &&     \
      (fabs(U2(expectedquat.v).y-U2(gotquat.v).y)<admit_error) &&     \
      (fabs(U3(expectedquat.v).z-U3(gotquat.v).z)<admit_error) &&     \
      (fabs(expectedquat.s-gotquat.s)<admit_error), \
  "Expected Quaternion %f %f %f %f , Got Quaternion %f %f %f %f\n", \
      expectedquat.s,U1(expectedquat.v).x,U2(expectedquat.v).y,U3(expectedquat.v).z, \
      gotquat.s,U1(gotquat.v).x,U2(gotquat.v).y,U3(gotquat.v).z);

#define expect_vec(expectedvec,gotvec) \
  ok( ((fabs(U1(expectedvec).x-U1(gotvec).x)<admit_error)&&(fabs(U2(expectedvec).y-U2(gotvec).y)<admit_error)&&(fabs(U3(expectedvec).z-U3(gotvec).z)<admit_error)), \
  "Expected Vector= (%f, %f, %f)\n , Got Vector= (%f, %f, %f)\n", \
  U1(expectedvec).x,U2(expectedvec).y,U3(expectedvec).z, U1(gotvec).x, U2(gotvec).y, U3(gotvec).z);

static HMODULE d3drm_handle = 0;

static void (WINAPI * pD3DRMMatrixFromQuaternion)(D3DRMMATRIX4D, LPD3DRMQUATERNION);
static LPD3DVECTOR (WINAPI* pD3DRMVectorAdd)(LPD3DVECTOR, LPD3DVECTOR, LPD3DVECTOR);
static LPD3DVECTOR (WINAPI* pD3DRMVectorCrossProduct)(LPD3DVECTOR, LPD3DVECTOR, LPD3DVECTOR);
static D3DVALUE (WINAPI* pD3DRMVectorDotProduct)(LPD3DVECTOR, LPD3DVECTOR);
static D3DVALUE (WINAPI* pD3DRMVectorModulus)(LPD3DVECTOR);
static LPD3DVECTOR (WINAPI * pD3DRMVectorNormalize)(LPD3DVECTOR);
static LPD3DVECTOR (WINAPI * pD3DRMVectorReflect)(LPD3DVECTOR, LPD3DVECTOR, LPD3DVECTOR);
static LPD3DVECTOR (WINAPI * pD3DRMVectorRotate)(LPD3DVECTOR, LPD3DVECTOR, LPD3DVECTOR, D3DVALUE);
static LPD3DVECTOR (WINAPI * pD3DRMVectorScale)(LPD3DVECTOR, LPD3DVECTOR, D3DVALUE);
static LPD3DVECTOR (WINAPI * pD3DRMVectorSubtract)(LPD3DVECTOR, LPD3DVECTOR, LPD3DVECTOR);
static LPD3DRMQUATERNION (WINAPI * pD3DRMQuaternionFromRotation)(LPD3DRMQUATERNION, LPD3DVECTOR, D3DVALUE);
static LPD3DRMQUATERNION (WINAPI * pD3DRMQuaternionSlerp)(LPD3DRMQUATERNION, LPD3DRMQUATERNION, LPD3DRMQUATERNION, D3DVALUE);
static D3DCOLOR (WINAPI * pD3DRMCreateColorRGB)(D3DVALUE, D3DVALUE, D3DVALUE);
static D3DCOLOR (WINAPI * pD3DRMCreateColorRGBA)(D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE);
static D3DVALUE (WINAPI * pD3DRMColorGetAlpha)(D3DCOLOR);
static D3DVALUE (WINAPI * pD3DRMColorGetBlue)(D3DCOLOR);
static D3DVALUE (WINAPI * pD3DRMColorGetGreen)(D3DCOLOR);
static D3DVALUE (WINAPI * pD3DRMColorGetRed)(D3DCOLOR);

#define D3DRM_GET_PROC(func) \
    p ## func = (void*)GetProcAddress(d3drm_handle, #func); \
    if(!p ## func) { \
      trace("GetProcAddress(%s) failed\n", #func); \
      FreeLibrary(d3drm_handle); \
      return FALSE; \
    }

static BOOL InitFunctionPtrs(void)
{
    d3drm_handle = LoadLibraryA("d3drm.dll");

    if(!d3drm_handle)
    {
        skip("Could not load d3drm.dll\n");
        return FALSE;
    }

    D3DRM_GET_PROC(D3DRMMatrixFromQuaternion)
    D3DRM_GET_PROC(D3DRMVectorAdd)
    D3DRM_GET_PROC(D3DRMVectorCrossProduct)
    D3DRM_GET_PROC(D3DRMVectorDotProduct)
    D3DRM_GET_PROC(D3DRMVectorModulus)
    D3DRM_GET_PROC(D3DRMVectorNormalize)
    D3DRM_GET_PROC(D3DRMVectorReflect)
    D3DRM_GET_PROC(D3DRMVectorRotate)
    D3DRM_GET_PROC(D3DRMVectorScale)
    D3DRM_GET_PROC(D3DRMVectorSubtract)
    D3DRM_GET_PROC(D3DRMQuaternionFromRotation)
    D3DRM_GET_PROC(D3DRMQuaternionSlerp)
    D3DRM_GET_PROC(D3DRMCreateColorRGB)
    D3DRM_GET_PROC(D3DRMCreateColorRGBA)
    D3DRM_GET_PROC(D3DRMColorGetAlpha)
    D3DRM_GET_PROC(D3DRMColorGetBlue)
    D3DRM_GET_PROC(D3DRMColorGetGreen)
    D3DRM_GET_PROC(D3DRMColorGetRed)

    return TRUE;
}


static void VectorTest(void)
{
    D3DVALUE mod,par,theta;
    D3DVECTOR e,r,u,v,w,axis,casnul,norm,ray,self;

    U1(u).x=2.0f; U2(u).y=2.0f; U3(u).z=1.0f;
    U1(v).x=4.0f; U2(v).y=4.0f; U3(v).z=0.0f;


/*______________________VectorAdd_________________________________*/
    pD3DRMVectorAdd(&r,&u,&v);
    U1(e).x=6.0f; U2(e).y=6.0f; U3(e).z=1.0f;
    expect_vec(e,r);

    U1(self).x=9.0f; U2(self).y=18.0f; U3(self).z=27.0f;
    pD3DRMVectorAdd(&self,&self,&u);
    U1(e).x=11.0f; U2(e).y=20.0f; U3(e).z=28.0f;
    expect_vec(e,self);

/*_______________________VectorSubtract__________________________*/
    pD3DRMVectorSubtract(&r,&u,&v);
    U1(e).x=-2.0f; U2(e).y=-2.0f; U3(e).z=1.0f;
    expect_vec(e,r);

    U1(self).x=9.0f; U2(self).y=18.0f; U3(self).z=27.0f;
    pD3DRMVectorSubtract(&self,&self,&u);
    U1(e).x=7.0f; U2(e).y=16.0f; U3(e).z=26.0f;
    expect_vec(e,self);

/*_______________________VectorCrossProduct_______________________*/
    pD3DRMVectorCrossProduct(&r,&u,&v);
    U1(e).x=-4.0f; U2(e).y=4.0f; U3(e).z=0.0f;
    expect_vec(e,r);

    U1(self).x=9.0f; U2(self).y=18.0f; U3(self).z=27.0f;
    pD3DRMVectorCrossProduct(&self,&self,&u);
    U1(e).x=-36.0f; U2(e).y=45.0f; U3(e).z=-18.0f;
    expect_vec(e,self);

/*_______________________VectorDotProduct__________________________*/
    mod=pD3DRMVectorDotProduct(&u,&v);
    ok((mod == 16.0f), "Expected 16.0f, Got %f\n", mod);

/*_______________________VectorModulus_____________________________*/
    mod=pD3DRMVectorModulus(&u);
    ok((mod == 3.0f), "Expected 3.0f, Got %f\n", mod);

/*_______________________VectorNormalize___________________________*/
    pD3DRMVectorNormalize(&u);
    U1(e).x=2.0f/3.0f; U2(e).y=2.0f/3.0f; U3(e).z=1.0f/3.0f;
    expect_vec(e,u);

/* If u is the NULL vector, MSDN says that the return vector is NULL. In fact, the returned vector is (1,0,0). The following test case prove it. */

    U1(casnul).x=0.0f; U2(casnul).y=0.0f; U3(casnul).z=0.0f;
    pD3DRMVectorNormalize(&casnul);
    U1(e).x=1.0f; U2(e).y=0.0f; U3(e).z=0.0f;
    expect_vec(e,casnul);

/*____________________VectorReflect_________________________________*/
    U1(ray).x=3.0f; U2(ray).y=-4.0f; U3(ray).z=5.0f;
    U1(norm).x=1.0f; U2(norm).y=-2.0f; U3(norm).z=6.0f;
    U1(e).x=79.0f; U2(e).y=-160.0f; U3(e).z=487.0f;
    pD3DRMVectorReflect(&r,&ray,&norm);
    expect_vec(e,r);

/*_______________________VectorRotate_______________________________*/
    U1(w).x=3.0f; U2(w).y=4.0f; U3(w).z=0.0f;
    U1(axis).x=0.0f; U2(axis).y=0.0f; U3(axis).z=1.0f;
    theta=2.0f*PI/3.0f;
    pD3DRMVectorRotate(&r,&w,&axis,theta);
    U1(e).x=-0.3f-0.4f*sqrtf(3.0f); U2(e).y=0.3f*sqrtf(3.0f)-0.4f; U3(e).z=0.0f;
    expect_vec(e,r);

/* The same formula gives D3DRMVectorRotate, for theta in [-PI/2;+PI/2] or not. The following test proves this fact.*/
    theta=-PI/4.0f;
    pD3DRMVectorRotate(&r,&w,&axis,theta);
    U1(e).x=1.4f/sqrtf(2.0f); U2(e).y=0.2f/sqrtf(2.0f); U3(e).z=0.0f;
    expect_vec(e,r);

    theta=PI/8.0f;
    pD3DRMVectorRotate(&self,&self,&axis,theta);
    U1(e).x=0.989950; U2(e).y=0.141421f; U3(e).z=0.0f;
    expect_vec(e,r);

/*_______________________VectorScale__________________________*/
    par=2.5f;
    pD3DRMVectorScale(&r,&v,par);
    U1(e).x=10.0f; U2(e).y=10.0f; U3(e).z=0.0f;
    expect_vec(e,r);

    U1(self).x=9.0f; U2(self).y=18.0f; U3(self).z=27.0f;
    pD3DRMVectorScale(&self,&self,2);
    U1(e).x=18.0f; U2(e).y=36.0f; U3(e).z=54.0f;
    expect_vec(e,self);
}

static void MatrixTest(void)
{
    D3DRMQUATERNION q;
    D3DRMMATRIX4D exp,mat;

    exp[0][0]=-49.0f; exp[0][1]=4.0f;   exp[0][2]=22.0f;  exp[0][3]=0.0f;
    exp[1][0]=20.0f;  exp[1][1]=-39.0f; exp[1][2]=20.0f;  exp[1][3]=0.0f;
    exp[2][0]=10.0f;  exp[2][1]=28.0f;  exp[2][2]=-25.0f; exp[2][3]=0.0f;
    exp[3][0]=0.0f;   exp[3][1]=0.0f;   exp[3][2]=0.0f;   exp[3][3]=1.0f;
    q.s=1.0f; U1(q.v).x=2.0f; U2(q.v).y=3.0f; U3(q.v).z=4.0f;

   pD3DRMMatrixFromQuaternion(mat,&q);
   expect_mat(exp,mat);
}

static void QuaternionTest(void)
{
    D3DVECTOR axis;
    D3DVALUE par,theta;
    D3DRMQUATERNION q,q1,q1final,q2,q2final,r;

/*_________________QuaternionFromRotation___________________*/
    U1(axis).x=1.0f; U2(axis).y=1.0f; U3(axis).z=1.0f;
    theta=2.0f*PI/3.0f;
    pD3DRMQuaternionFromRotation(&r,&axis,theta);
    q.s=0.5f; U1(q.v).x=0.5f; U2(q.v).y=0.5f; U3(q.v).z=0.5f;
    expect_quat(q,r);

/*_________________QuaternionSlerp_________________________*/
/* If the angle of the two quaternions is in ]PI/2;3PI/2[, QuaternionSlerp
 * interpolates between the first quaternion and the opposite of the second one.
 * The test proves this fact. */
    par=0.31f;
    q1.s=1.0f; U1(q1.v).x=2.0f; U2(q1.v).y=3.0f; U3(q1.v).z=50.0f;
    q2.s=-4.0f; U1(q2.v).x=6.0f; U2(q2.v).y=7.0f; U3(q2.v).z=8.0f;
/* The angle between q1 and q2 is in [-PI/2,PI/2]. So, one interpolates between q1 and q2. */
    q.s = -0.55f; U1(q.v).x=3.24f; U2(q.v).y=4.24f; U3(q.v).z=36.98f;
    pD3DRMQuaternionSlerp(&r,&q1,&q2,par);
    expect_quat(q,r);

    q1.s=1.0f; U1(q1.v).x=2.0f; U2(q1.v).y=3.0f; U3(q1.v).z=50.0f;
    q2.s=-94.0f; U1(q2.v).x=6.0f; U2(q2.v).y=7.0f; U3(q2.v).z=-8.0f;
/* The angle between q1 and q2 is not in [-PI/2,PI/2]. So, one interpolates between q1 and -q2. */
    q.s=29.83f; U1(q.v).x=-0.48f; U2(q.v).y=-0.10f; U3(q.v).z=36.98f;
    pD3DRMQuaternionSlerp(&r,&q1,&q2,par);
    expect_quat(q,r);

/* Test the spherical interpolation part */
    q1.s=0.1f; U1(q1.v).x=0.2f; U2(q1.v).y=0.3f; U3(q1.v).z=0.4f;
    q2.s=0.5f; U1(q2.v).x=0.6f; U2(q2.v).y=0.7f; U3(q2.v).z=0.8f;
    q.s = 0.243943f; U1(q.v).x = 0.351172f; U2(q.v).y = 0.458401f; U3(q.v).z = 0.565629f;

    q1final=q1;
    q2final=q2;
    pD3DRMQuaternionSlerp(&r,&q1,&q2,par);
    expect_quat(q,r);

/* Test to show that the input quaternions are not changed */
    expect_quat(q1,q1final);
    expect_quat(q2,q2final);
}

static void ColorTest(void)
{
    D3DCOLOR color, expected_color, got_color;
    D3DVALUE expected, got, red, green, blue, alpha;

/*___________D3DRMCreateColorRGB_________________________*/
    red=0.8f;
    green=0.3f;
    blue=0.55f;
    expected_color=0xffcc4c8c;
    got_color=pD3DRMCreateColorRGB(red,green,blue);
    ok((expected_color==got_color),"Expected color=%x, Got color=%x\n",expected_color,got_color);

/*___________D3DRMCreateColorRGBA________________________*/
    red=0.1f;
    green=0.4f;
    blue=0.7f;
    alpha=0.58f;
    expected_color=0x931966b2;
    got_color=pD3DRMCreateColorRGBA(red,green,blue,alpha);
    ok((expected_color==got_color),"Expected color=%x, Got color=%x\n",expected_color,got_color);

/* if a component is <0 then, then one considers this component as 0. The following test proves this fact (test only with the red component). */
    red=-0.88f;
    green=0.4f;
    blue=0.6f;
    alpha=0.41f;
    expected_color=0x68006699;
    got_color=pD3DRMCreateColorRGBA(red,green,blue,alpha);
    ok((expected_color==got_color),"Expected color=%x, Got color=%x\n",expected_color,got_color);

/* if a component is >1 then, then one considers this component as 1. The following test proves this fact (test only with the red component). */
    red=2.37f;
    green=0.4f;
    blue=0.6f;
    alpha=0.41f;
    expected_color=0x68ff6699;
    got_color=pD3DRMCreateColorRGBA(red,green,blue,alpha);
    ok((expected_color==got_color),"Expected color=%x, Got color=%x\n",expected_color,got_color);

/*___________D3DRMColorGetAlpha_________________________*/
    color=0x0e4921bf;
    expected=14.0f/255.0f;
    got=pD3DRMColorGetAlpha(color);
    ok((fabs(expected-got)<admit_error),"Expected=%f, Got=%f\n",expected,got);

/*___________D3DRMColorGetBlue__________________________*/
    color=0xc82a1455;
    expected=1.0f/3.0f;
    got=pD3DRMColorGetBlue(color);
    ok((fabs(expected-got)<admit_error),"Expected=%f, Got=%f\n",expected,got);

/*___________D3DRMColorGetGreen_________________________*/
    color=0xad971203;
    expected=6.0f/85.0f;
    got=pD3DRMColorGetGreen(color);
    ok((fabs(expected-got)<admit_error),"Expected=%f, Got=%f\n",expected,got);

/*___________D3DRMColorGetRed__________________________*/
    color=0xb62d7a1c;
    expected=3.0f/17.0f;
    got=pD3DRMColorGetRed(color);
    ok((fabs(expected-got)<admit_error),"Expected=%f, Got=%f\n",expected,got);
}

START_TEST(vector)
{
    if(!InitFunctionPtrs())
        return;

    VectorTest();
    MatrixTest();
    QuaternionTest();
    ColorTest();

    FreeLibrary(d3drm_handle);
}
