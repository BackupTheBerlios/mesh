/* $Id: compute_error.h,v 1.8 2001/08/08 14:31:33 dsanta Exp $ */
#include <3dmodel.h>

#ifndef _COMPUTE_ERROR_PROTO
#define _COMPUTE_ERROR_PROTO

#ifdef __cplusplus
#define BEGIN_DECL extern "C" {
#define END_DECL }
#else
#define BEGIN_DECL
#define END_DECL
#endif

BEGIN_DECL
#undef BEGIN_DECL

/* --------------------------------------------------------------------------*
 *                       Exported data types                                 *
 * --------------------------------------------------------------------------*/

/* A integer size in 3D */
struct size3d {
  int x; /* Number of elements in the X direction */
  int y; /* Number of elements in the Y direction */
  int z; /* Number of elements in the Z direction */
};

/* A list of model faces */
struct face_list {
  int *face;   /* Array of indices of the faces in the list */
  int n_faces; /* Number of faces in the array */
};

/* Per face error metrics */
struct face_error {
  double face_area;      /* Area of the face, for error weighting in averages */
  double min_error;      /* The minimum error for the face */
  double max_error;      /* The maximum error for the face */
  double mean_error;     /* The mean error for the face */
  double mean_sqr_error; /* The mean squared error for the face */
};

/* Statistics from the dist_surf_surf function */
struct dist_surf_surf_stats {
  double m1_area;   /* Area of model 1 surface */
  double m2_area;   /* Area of model 2 surface */
  double min_dist;  /* Minimum distance from model 1 to model 2 */
  double max_dist;  /* Maximum distance from model 1 to model 2 */
  double mean_dist; /* Mean distance from model 1 to model 2 */
  double rms_dist;  /* Root mean squared distance from model 1 to model 2 */
  double cell_sz;   /* The partitioning cubic cell side length */
  struct size3d grid_sz; /* The number of cells in the partitioning grid in
                          * each direction X,Y,Z */
};

/* --------------------------------------------------------------------------*
 *                       Exported functions                                  *
 * --------------------------------------------------------------------------*/

/* Returns an array of length m->num_vert with the list of faces incident on
 * each vertex. */
struct face_list *faces_of_vertex(model *m);

/* Calculates the distance from model m1 to model m2. The triangles of m1 are
 * sampled using n_spt samples in each direction. The per face (of m1) error
 * metrics are returned in a new array (of length m1->num_faces) allocated at
 * *fe_ptr. The overall distance metrics and other statistics are returned in
 * stats. If quiet is zero a progress meter is displayed in stdout. */
void dist_surf_surf(const model *m1, const model *m2, int n_spt,
                    struct face_error *fe_ptr[],
                    struct dist_surf_surf_stats *stats, int quiet);

END_DECL
#undef END_DECL

#endif /* _COMPUTE_ERROR_PROTO */
