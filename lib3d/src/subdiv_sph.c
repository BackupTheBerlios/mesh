/* $Id: subdiv_sph.c,v 1.15 2003/04/28 06:20:09 aspert Exp $ */
#include <3dutils.h>
#include <3dmodel.h>
#include <normals.h>
#include <geomutils.h>
#include <subdiv_methods.h>
#include <assert.h>
#if defined(SUBDIV_SPH_DEBUG) || defined(DEBUG)
# include <debug_print.h>
#endif

#define EPS 1e-10f
#define DEG(x) ((x)*180.0/M_PI)
/* 5*Pi/8 */
#define M_5_PI_8 1.96349540849362077404
/* 3*Pi/8 */ 
#define M_3_PI_8 1.17809724509617246442
/* Pi/8 */
#define M_PI_8   0.39269908169872415481

/* ph -> h(ph) */
float h_orig(const float x) 
{
  float tmp, res;
  if (x <= -M_PI_2 || x >= M_PI_2)
    res = x;
  else if (x < -M_PI_4) {
    tmp = x + M_PI_4;
    res = -M_1_PI*tmp*tmp*(24.0*M_1_PI*tmp + 10) + 0.5*x;
  }
  else if (x <= M_PI_4)
    res = 0.5*x;
  else {
    tmp = x - M_PI_4;
    res = M_1_PI*tmp*tmp*(-24.0*M_1_PI*tmp + 10) + 0.5*x;
  }
  return res;
}


float h_alt(const float x)
{
  float res;
  if (x <= -M_5_PI_8 || x>= M_5_PI_8)
    res = x;
  else if (x < -M_3_PI_8)
    res = (64*x*x + 144*M_PI*x + 25*M_PI*M_PI)/(64*M_PI);
  else if (x < -M_PI_8)
    res = -(64*x*x + M_PI*M_PI)/(32*M_PI);
  else if (x <= M_PI_8)
    res = 0.5*x;
  else if (x <= M_3_PI_8)
    res = (64*x*x + M_PI*M_PI)/(32*M_PI);
  else /*if (x < M_5_PI_8) */
    res = (-64*x*x + 144*M_PI*x - 25*M_PI*M_PI)/(64*M_PI);
    
  return res;
}

static void half_sph(const vertex_t *p, 
                     const vertex_t *n, 
                     const vertex_t *q, 
		     float (*h_func)(const float),
		     vertex_t *vout)
{
  float r, dz, th, nth, rp, lambda, pl_off, nr;
  vertex_t dir, m, u, v, np;

  pl_off = -scalprod_v(p, n);
  substract_v(q, p, &dir);

  r = __norm_v(dir);

  lambda = -(pl_off + scalprod_v(q, n));
  prod_v(lambda, n, &m);

  /* u = projection of vj into the plane defined by \vec{n} */
  add_v(q, &m, &u);
  /* v = vector from u to p (i.e. in the plane defined by \vec{n}) */ 
  substract_v(&u, p, &v);
  
  /* Sanity check for curve subdivision mostly */
  /* some cases can happen where ||v||=0 */
  /* It sucks. Let's take the midpoint of the edge */
  if (__norm_v(v) < EPS) {
    add_v(p, q, &np);
    prod_v(0.5f, &np, vout);
#ifdef SUBDIV_SPH_DEBUG
    DEBUG_PRINT("Ouch");
#endif
    return;
  }

  if (lambda >= 0.0)
    th = -atan(__norm_v(m)/__norm_v(v));
  else
    th = atan(__norm_v(m)/__norm_v(v));

  nr = 0.5*r;
  nth = h_func(th);

  dz = nr*sin(nth);
  rp = nr*cos(nth);

  __normalize_v(v);
  prod_v(rp, &v, &np);

  /* np += dz*n + p */
  add_prod_v(dz, n, &np, &np);
  add_v(&np, p, vout);

#ifdef SUBDIV_SPH_DEBUG
  printf("p = %f %f %f\n", p->x, p->y, p->z);
  printf("n = %f %f %f\n", n->x, n->y, n->z);
  printf("q = %f %f %f\n", q->x, q->y, q->z);
  printf("pl_off = %f r = %f\n", pl_off, r);
  printf("th = %f nth = %f\n", DEG(th), DEG(nth));
  printf("u.n + d = %f\n", scalprod_v(&u,n)+pl_off);
#endif



}   

void compute_midpoint_sph(const struct ring_info *rings, const int center, 
                          const int v1, 
			  const struct model *raw_model, 
			  float (*h_func)(const float),
			  vertex_t *vout) 
{

  int center2 = rings[center].ord_vert[v1];
  struct ring_info ring_op = rings[center2];
  int v2 = 0;
  vertex_t n, p, vj, np1, np2, np;


  n = raw_model->normals[center];
  p = raw_model->vertices[center];
  vj = raw_model->vertices[center2];
  
#ifdef SUBDIV_SPH_DEBUG
  DEBUG_PRINT("Edge %d %d\n", center, center2);
#endif
  half_sph(&p, &n, &vj, h_func, &np1);


  while (ring_op.ord_vert[v2] != center)
      v2++;
  
  n = raw_model->normals[center2];
  p = raw_model->vertices[center2];
  vj = raw_model->vertices[center];

#ifdef SUBDIV_SPH_DEBUG
  DEBUG_PRINT("Edge %d %d\n", center2, center);
#endif
  half_sph(&p, &n, &vj, h_func, &np2);

  __add_v(np1, np2, np);
  prod_v(0.5, &np, vout);


}



void compute_midpoint_sph_crease(const struct ring_info *rings, 
                                 const int center, const int v1, 
                                 const struct model *raw_model, 
				 float (*h_func)(const float),
                                 vertex_t *vout)
{
  int n_r = rings[center].size;
  struct ring_info ring = rings[center];
  int center2 = ring.ord_vert[v1];
  struct ring_info ring_op = rings[center2];
  int nrop;
  int v3 = -1, f1 = -1, f2 = -1;
  int v2 = 0;
  vertex_t p, q, np, a, b, ns1, ns2, n, np1, np2;
  float r1, r2;

  p = raw_model->vertices[center];
  q = raw_model->vertices[center2];

  nrop = ring_op.size;
  while (ring_op.ord_vert[v2] != center)
    v2++;

  if (ring.type == 1 && ring_op.type == 1) { /* boundary here */
    /* go backward */
    if (ring.ord_vert[0] == center2) {
      v3 = ring.ord_vert[n_r-1];
      f1 = ring.ord_face[0]; /* Just in case things fail ;-) */
      f2 = ring.ord_face[n_r-2]; /* should be OK since we do not allow
                                  * isolated vertices */
    }
    else if (ring.ord_vert[n_r-1] == center2) {
      v3 = ring.ord_vert[0];
      f2 = ring.ord_face[0];
      f1 = ring.ord_face[n_r-2]; 
    }
    else { /* we have a non-boundary -> midpoint */
      add_v(&p, &q, &np);
      prod_v(0.5, &np, vout);
      return;
    }

    __substract_v(raw_model->vertices[v3], p, a);
    r1 = __norm_v(a);
    __substract_v(q, p, b);
    r2 = __norm_v(b);
    __crossprod_v(a, b, np);
    if(__norm_v(np)<EPS) { /* a and b are probably colinear... it
                            * sucks, so let's fall back to another
                            * method */
#ifdef SUBDIV_SPH_DEBUG
    DEBUG_PRINT("a = {%f %f %f}\n", a.x, a.y, a.z);
    DEBUG_PRINT("b = {%f %f %f}\n", b.x, b.y, b.z);
    DEBUG_PRINT("np = {%f %f %f}\n", np.x, np.y, np.z);
#endif
      /* Get approx. normal to plane (center,center2,v2) */
      __prod_v(raw_model->area[f1], raw_model->face_normals[f1], np);
      __add_prod_v(raw_model->area[f2], raw_model->face_normals[f2], np, np);
      __normalize_v(np);
    } 
    else 
      __normalize_v(np);

    /* get side normals */
    __crossprod_v(a, np, ns1);
    __normalize_v(ns1);
    __crossprod_v(np, b, ns2);
    __normalize_v(ns2);

    /* Now get the normal est. at vertex 'center' */
    __prod_v(r1, ns1, n);
    __add_prod_v(r2, ns2, n, n);
    __normalize_v(n);

#ifdef SUBDIV_SPH_DEBUG
  DEBUG_PRINT("Edge %d %d\n", center, center2);
#endif
    /* Now proceed through a usual spherical subdivision */
    half_sph(&p, &n, &q, h_func, &np1);

    /* go forward */
    if (ring_op.ord_vert[0] == center) {
      v3 = ring_op.ord_vert[nrop-1];
      f1 = ring_op.ord_face[0];
      f2 = ring_op.ord_face[nrop-2];
    }
    else if (ring_op.ord_vert[nrop-1] == center) {
      v3 = ring_op.ord_vert[0];
      f1 = ring_op.ord_face[0];
      f2 = ring_op.ord_face[nrop-2];
    }
    else {
      __add_v(p, q, np);
      prod_v(0.5f, &np, vout);
      return;
    }

    __substract_v(p, q, a);
    r1 = r2;
    __substract_v(raw_model->vertices[v3], q, b);
    r2 = __norm_v(b);
    __crossprod_v(a, b, np);

    if(__norm_v(np)<EPS) { /* a and b are probably colinear... it
                            * sucks, so let's fall back to another
                            * method */
#ifdef SUBDIV_SPH_DEBUG
    DEBUG_PRINT("a = {%f %f %f}\n", a.x, a.y, a.z);
    DEBUG_PRINT("b = {%f %f %f}\n", b.x, b.y, b.z);
    DEBUG_PRINT("np = {%f %f %f}\n", np.x, np.y, np.z);
#endif
      /* Get the normal to the plane (center, center2, v3) */
      __prod_v(raw_model->area[f1], raw_model->face_normals[f1], np);
      __add_prod_v(raw_model->area[f2], raw_model->face_normals[f2], np, np);
      __normalize_v(np);
    } 
    else 
      __normalize_v(np);

    /* get side normals */
    __crossprod_v(a, np, ns1);
    __normalize_v(ns1);
    __crossprod_v(np, b, ns2);
    __normalize_v(ns2);
#ifdef SUBDIV_SPH_DEBUG
    DEBUG_PRINT("ns1 = {%f %f %f}\n", ns1.x, ns1.y, ns1.z);
    DEBUG_PRINT("ns2 = {%f %f %f}\n", ns2.x, ns2.y, ns2.z);
#endif
    /* Now get the normal est. at vertex 'center2' */
    __prod_v(r1, ns1, n);
    __add_prod_v(r2, ns2, n, n);
    __normalize_v(n);

#ifdef SUBDIV_SPH_DEBUG
  DEBUG_PRINT("Edge %d %d\n", center2, center);
#endif
    /* Perform sph. subdivision */
    half_sph(&q, &n, &p, h_func, &np2);

    /* gather those new points */
    __add_v(np1, np2, np);
    prod_v(0.5f, &np, vout);

  } else if (ring.type == 1)  /* && ring_op.type == 0 */
    compute_midpoint_sph(rings, center2, v2, raw_model, h_func, vout);
  else  /* ring_op.type == 1 && ring.type == 0 */
    compute_midpoint_sph(rings, center, v1, raw_model, h_func, vout);


}





