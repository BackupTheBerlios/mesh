/* $Id: 3dmodel.h,v 1.27 2002/02/21 13:35:23 aspert Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
/* What follows is to be able to compile */
/* with '-ansi' flag and still use the */
/* useful constants from 'math.h' */
#ifndef __USE_BSD
# define __USE_BSD
#endif
#include <math.h>
#include <limits.h>
#include <float.h>

/* Make sure we have the mathematical constants defined */
#ifndef M_E
# define M_E		2.7182818284590452354	/* e */
#endif
#ifndef M_LOG2E
# define M_LOG2E	1.4426950408889634074	/* log_2 e */
#endif
#ifndef M_LOG10E
# define M_LOG10E	0.43429448190325182765	/* log_10 e */
#endif
#ifndef M_LN2
# define M_LN2		0.69314718055994530942	/* log_e 2 */
#endif
#ifndef M_LN10
# define M_LN10		2.30258509299404568402	/* log_e 10 */
#endif
#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif
#ifndef M_PI_2
# define M_PI_2		1.57079632679489661923	/* pi/2 */
#endif
#ifndef M_PI_4
# define M_PI_4		0.78539816339744830962	/* pi/4 */
#endif
#ifndef M_1_PI
# define M_1_PI		0.31830988618379067154	/* 1/pi */
#endif
#ifndef M_2_PI
# define M_2_PI		0.63661977236758134308	/* 2/pi */
#endif
#ifndef M_2_SQRTPI
# define M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#endif
#ifndef M_SQRT2
# define M_SQRT2	1.41421356237309504880	/* sqrt(2) */
#endif
#ifndef M_SQRT1_2
# define M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */
#endif

#ifndef _3D_MODEL
#define _3D_MODEL

typedef struct {
  float x;
  float y;
  float z;
}vertex_t;

typedef struct {
  double x;
  double y;
  double z;
}dvertex_t;

typedef struct {
  int f0;
  int f1;
  int f2;
}face_t;



struct info_vertex{
  int num_faces;
  int *list_face; /* Index of faces containing the vertex */
  int outdegree;
  int *list_vertex; /* Index of vertices neighbouring the current vertex */
  double mixed_area;
  double gauss_curv, mean_curv;
  vertex_t mean_curv_normal;
  double c[3];
  double k1, k2; /* principal curvature */
  vertex_t t1,t2; /* principal directions (if any) */
};


struct edge_v {
  int v0;
  int v1;
  int face;
};


struct face_tree {
  int face_idx;
  int visited;
  int node_type; /* 0 -> left_child 1 -> right_child */
  struct edge_v prim_left;
  struct edge_v prim_right;
  struct face_tree *left;
  struct face_tree *right;
  struct face_tree *parent;
  int v0,v1,v2;
};


struct model {
  int num_faces;
  int num_vert;
  int builtin_normals; /* 1 if normals are already in the file 0 otherwise */
  vertex_t *vertices;
  vertex_t *normals; /* Normals for each vertex of the model */
  vertex_t *face_normals;
  face_t *faces;
  float *area; /* area of each face */
  float total_area; /* area of the whole model */
  vertex_t bBox[2]; /* bBox[0] is the min  bBox[1] is the max */
  struct face_tree **tree; /* spanning tree of the dual graph */
};

#endif
