/* $Id: compute_error.c,v 1.56 2001/09/10 15:09:53 dsanta Exp $ */

#include <compute_error.h>

#include <geomutils.h>
#include <xalloc.h>
#include <math.h>
#include <assert.h>

/* Use a bitmap for marking empty cells. Otherwise use array of a simple
 * type. Using a bitmap uses less memory and can be faster than a simple type
 * depending on the cache system and number of cells in the grid (number of
 * cache misses). */
#define USE_EC_BITMAP

/* Ratio used to derive the cell size. It is the ratio between the cubic cell
 * side length and the side length of an average equilateral triangle. */
#define CELL_TRIAG_RATIO 3

/* If defined statistics for the dist_pt_surf() function are computed */
/* #define DO_DIST_PT_SURF_STATS */

/* Margin factor from DBL_MIN to consider a triangle side length too small and
 * mark it as degenerate. */
#define DMARGIN 1e10

/* Maximum number of cells in the grid. */
#define GRID_CELLS_MAX 512000

/* The value of 1/sqrt(3) */
#define SQRT_1_3 0.5773502691896258

/* Define inlining directive for C99 or as compiler specific C89 extension */
#if defined(__GNUC__) /* GCC's interpretation is inverse of C99 */
# define INLINE __inline__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
# define INLINE inline
#else
# define INLINE /* no inline */
#endif

/* --------------------------------------------------------------------------*
 *                       Local data types                                    *
 * --------------------------------------------------------------------------*/

#ifdef USE_EC_BITMAP /* Use a bitmap */
  /* Type for storing empty cell bitmap */
  typedef int ec_bitmap_t;
  /* The number of bytes used by ec_bitmap_t */
# define EC_BITMAP_T_SZ (sizeof(ec_bitmap_t))
  /* The number of bits used by ec_bitmap_t, and also the divisor to obtain the
   * bitmap element index from a cell index */
# define EC_BITMAP_T_BITS (EC_BITMAP_T_SZ*8)
  /* Bitmask to obtain the bitmap bit index from the cell index. */
# define EC_BITMAP_T_MASK (EC_BITMAP_T_BITS-1)
  /* Macro to test the bit corresponding to element i in the bitmap bm. */
# define EC_BITMAP_TEST_BIT(bm,i) \
   (bm[i/EC_BITMAP_T_BITS]&(0x01<<(i&EC_BITMAP_T_MASK)))
  /* Macro to set the bit corresponding to element i in the bitmap bm. */
# define EC_BITMAP_SET_BIT(bm,i) \
   (bm[i/EC_BITMAP_T_BITS] |= 0x01<<(i&EC_BITMAP_T_MASK))
#else /* Fake bitmap macros to access simple type */
  /* Type for marking empty cells. Small type uses less memory, but access to
   * aligned type can be faster (but more memory can cause more cache misses) */
  typedef char ec_bitmap_t;
  /* The number of bytes used by ec_bitmap_t */
# define EC_BITMAP_T_SZ sizeof(ec_bitmap_t)
  /* The number of bits used by ec_bitmap_t. Since here we don't use the
   * individual bits it is 1. */
# define EC_BITMAP_T_BITS 1
  /* Bitmask to obtain the bitmap bit index from the cell index. */
# define EC_BITMAP_T_MASK (EC_BITMAP_T_BITS-1)
  /* Macro to test the element i in the map bm. */
# define EC_BITMAP_TEST_BIT(bm,i) bm[i]
  /* Macro to set the element i in the map bm. */
# define EC_BITMAP_SET_BIT(bm,i) (bm[i] = 1)
#endif

/* List of triangles intersecting each cell */
struct t_in_cell_list {
  int ** triag_idx;         /* The list of the indices of the triangles
                             * intersecting each cell, terminated by -1
                             * (triag_idx[i] is the list for the cell with
                             * linear index i). If cell i is empty,
                             * triag_idx[i] is NULL. */
  int n_cells;              /* The number of cells in triag_idx */
  ec_bitmap_t *empty_cell;  /* A bitmap indicating which cells are empty. If
                             * cell i is empty, the bit (i&EC_BITMAP_T_MASK)
                             * of empty_cell[i/EC_BITMAP_T_BITS] is
                             * non-zero. */
};

/* A list of samples of a surface in 3D space. */
struct sample_list {
  vertex* sample; /* Array of sample 3D coordinates */
  int n_samples;  /* The number of samples in the array */
};

/* A list of cells */
struct cell_list {
  int *cell;   /* The array of the linear indices of the cells in the list */
  int n_cells; /* The number of elemnts in the array */
};

/* A list of cells for different distances */
struct dist_cell_lists {
  struct cell_list *list; /* list[k]: the list of cells at distance k in the
                           * X, Y or Z direction. */
  int n_dists;            /* The number of elements in list */
};

/* Storage for triangle sample errors. */
struct triag_sample_error {
  double **err;      /* Error array with 2D addressing. Sample (i,j) has the
                      * error stored at err[i][j], where i varies betwen 0 and
                      * n_samples-1 inclusive and j varies between 0 and
                      * n_samples-i-1 inclusive. */
  int n_samples;     /* The number of samples in each triangle direction */
  double *err_lin;   /* Error array with 1D adressing, which varies from 0 to
                      * n_samples_tot-1 inclusive. It refers to the same
                      * location as err, thus any change to err is reflected
                      * in err_lin and vice-versa. The order in the 1D array
                      * is all errors for i equal 0 and j from 0 to
                      * n_samples-1, followed by errors for i equal 1 and j
                      * from 1 to n_samples-2, and so on. */
  int n_samples_tot; /* The total number of samples in the triangle */
  int buf_sz;        /* The allocated err_lin buffer size (number of elements)*/
};

/* A list of triangles with their associated information */
struct triangle_list {
  struct triangle_info *triangles; /* The triangles */
  int n_triangles;                 /* The number of triangles */
  double area;                     /* The total triangle area */
};

/* A triangle and useful associated information. AB is always the longest side
 * of the triangle. That way the projection of C on AB is always inside AB. */
struct triangle_info {
  vertex a;            /* The A vertex of the triangle */
  vertex b;            /* The B vertex of the triangle */
  vertex c;            /* The C vertex of the triangle. The projection of C
                        * on AB is always inside the AB segment. */
  vertex ab;           /* The AB vector */
  vertex ca;           /* The CA vector */
  vertex cb;           /* The CB vector */
  double ab_len_sqr;   /* The square of the length of AB */
  double ca_len_sqr;   /* The square of the length of CA */
  double cb_len_sqr;   /* The square of the length of CB */
  double ab_1_len_sqr; /* One over the square of the length of AB */
  double ca_1_len_sqr; /* One over the square of the length of CA */
  double cb_1_len_sqr; /* One over the square of the length of CB */
  vertex normal;       /* The (unit length) normal of the ABC triangle
                        * (orinted with the right hand rule turning from AB to
                        * AC). If the triangle is degenerate it is (0,0,0). */
  vertex nhsab;        /* (unnormalized) normal of the plane trough AB,
                        * perpendicular to ABC and pointing outside of ABC */
  vertex nhsbc;        /* (unnormalized) normal of the plane trough BC,
                        * perpendicular to ABC and pointing outside of ABC */
  vertex nhsca;        /* (unnormalized) normal of the plane trough CA,
                        * perpendicular to ABC and pointing outside of ABC */
  double chsab;        /* constant of the plane equation: <p|npab>=cpab */
  double chsbc;        /* constant of the plane equation: <p|npbc>=cpbc */
  double chsca;        /* constant of the plane equation: <p|npca>=cpca */
  double a_n;          /* scalar product of A with the unit length normal */
  int wide_at_c;       /* Flag indicating if the angle at C is larger than 90
                        * degrees */
  double s_area;       /* The surface area of the triangle */
};

/* Statistics of dist_pt_surf() function */
struct dist_pt_surf_stats {
  int n_cell_scans;       /* Number of cells that are scanned (i.e. distance
                           * point to cell is calculated) */
  int n_cell_t_scans;     /* Number of cells that for which their triangles are
                           * scanned */
  int n_triag_scans;      /* Number of triangles that are scanned */
  int sum_kmax;           /* the sum of athe max k for each sample point */
};

/* --------------------------------------------------------------------------*
 *                    Local utility functions                                *
 * --------------------------------------------------------------------------*/

/* Returns, in vout, the negative of vector v. */
static INLINE void neg_v(const vertex *v, vertex *vout)
{
  vout->x = -v->x;
  vout->y = -v->y;
  vout->z = -v->z;
}

/* Reallocates the buffers of tse to store the sample errors for a triangle
 * sampling with n samples in each direction. If tse->err and tse->err_lin is
 * NULL new buffers are allocated. The allocation never fails (if out of
 * memory the program is stopped, as with xa_realloc()) */
static void realloc_triag_sample_error(struct triag_sample_error *tse, int n)
{
  int i;

  tse->n_samples = n;
  tse->n_samples_tot = n*(n+1)/2;
  if (tse->n_samples_tot >  tse->buf_sz) {
    tse->err = xa_realloc(tse->err,n*sizeof(*(tse->err)));
    tse->err_lin = xa_realloc(tse->err_lin,
                              tse->n_samples_tot*sizeof(**(tse->err)));
    tse->buf_sz = tse->n_samples_tot;
  }
  if (n != 0) {
    tse->err[0] = tse->err_lin;
    for (i=1; i<n; i++) {
      tse->err[i] = tse->err[i-1]+(n-(i-1));
    }
  }
}

/* Frees the buffers in tse (allocated with realloc_triag_sample_error()). */
static void free_triag_sample_error(struct triag_sample_error *tse)
{
  if (tse == NULL) return;
  free(tse->err);
  free(tse->err_lin);
  tse->err = NULL;
  tse->err_lin = NULL;
}

/* Computes the vertex normals assuming an oriented model. The triangle
 * information already present in tl are used to speed up the calculation. If
 * the model is not oriented, the resulting normals will be incorrect. */
static void calc_normals_as_oriented_model(model *m,
                                           const struct triangle_list *tl)
{
  int k,kmax;
  vertex *n;

  /* initialize all normals to zero */
  m->normals = xa_realloc(m->normals,m->num_vert*sizeof(*(m->normals)));
  memset(m->normals,0,m->num_vert*sizeof(*(m->normals)));
  /* add face normals to vertices, weighted by face area */
  for (k=0, kmax=m->num_faces; k < kmax; k++) {
    n = &(tl->triangles[k].normal);
    prod_v(tl->triangles[k].s_area,n,n);
    add_v(n,&(m->normals[m->faces[k].f0]),&(m->normals[m->faces[k].f0]));
    add_v(n,&(m->normals[m->faces[k].f1]),&(m->normals[m->faces[k].f1]));
    add_v(n,&(m->normals[m->faces[k].f2]),&(m->normals[m->faces[k].f2]));
  }
  /* normalize final normals */
  for (k=0, kmax=m->num_vert; k<kmax; k++) {
    normalize_v(&(m->normals[k]));
  }
}

/* Returns the sampling frequency for a triangle given by the 3 vertices *a *b
 * *c, so that the distance between two samples on the longest side of the
 * triangle is not larger than step, and as close as possible. Note that,
 * depending on the triangle's shape, the distance between samples along other
 * sides might be much shorter than step. */
static int get_sampling_freq(const vertex *a, const vertex *b, const vertex *c,
                             double step)
{
  double ab_len_sqr;
  double ac_len_sqr;
  double bc_len_sqr;
  double max_len_sqr;

  /* Search for longest side */
  ab_len_sqr = dist2_v(a,b);
  ac_len_sqr = dist2_v(a,c);
  bc_len_sqr = dist2_v(b,c);
  max_len_sqr = max3(ab_len_sqr,ac_len_sqr,bc_len_sqr);
  /* Return the sampling frequency */
  return (int)floor(sqrt(max_len_sqr)/step)+1;
}

/* Given a the triangle list tl of a model, and the minimum and maximum
 * coordinates of the bounding box on which the cell grid is to be made,
 * bbox_min and bbox_max, calculates the grid cell size as well as the grid
 * size. The cubic cell side length is returned and the grid size is stored in
 * *grid_sz. */
static double get_cell_size(const struct triangle_list *tl,
                            const vertex *bbox_min, const vertex *bbox_max,
                            struct size3d *grid_sz)
{
  double cell_sz;
  double f_gsz_x,f_gsz_y,f_gsz_z;

  /* Derive the grid size. For that we derive the average triangle side length
   * as the side of an equilateral triangle which's surface equals the average
   * triangle surface of m2. The cubic cell side is then CELL_TRIAG_RATIO
   * times that. */
  cell_sz = CELL_TRIAG_RATIO*sqrt(tl->area/tl->n_triangles*2/sqrt(3));

  /* Avoid values that can overflow or underflow */
  if (cell_sz < DBL_MIN*DMARGIN) { /* Avoid division by zero with cell_sz */
    cell_sz = DBL_MIN*DMARGIN;
  } else if (cell_sz >= DBL_MAX/DMARGIN) {
    fprintf(stderr,"ERROR: coordinate overflow. Are models OK?\n");
    exit(1);
  }

  /* Limit to maximum number of cells */
  f_gsz_x = ceil((bbox_max->x-bbox_min->x)/cell_sz);
  if (f_gsz_x <= 0) f_gsz_x = 1;
  f_gsz_y = ceil((bbox_max->y-bbox_min->y)/cell_sz);
  if (f_gsz_y <= 0) f_gsz_y = 1;
  f_gsz_z = ceil((bbox_max->z-bbox_min->z)/cell_sz);
  if (f_gsz_z <= 0) f_gsz_z = 1;
  if (f_gsz_x*f_gsz_y*f_gsz_z > GRID_CELLS_MAX) {
    cell_sz = pow(f_gsz_x*f_gsz_y*f_gsz_z/GRID_CELLS_MAX,1/3);
    f_gsz_x = ceil((bbox_max->x-bbox_min->x)/cell_sz);
    if (f_gsz_x <= 0) f_gsz_x = 1;
    f_gsz_y = ceil((bbox_max->y-bbox_min->y)/cell_sz);
    if (f_gsz_y <= 0) f_gsz_y = 1;
    f_gsz_z = ceil((bbox_max->z-bbox_min->z)/cell_sz);
    if (f_gsz_z <= 0) f_gsz_z = 1;
  }

  /* Return values */
  grid_sz->x = (int) f_gsz_x;
  grid_sz->y = (int) f_gsz_y;
  grid_sz->z = (int) f_gsz_z;
  return cell_sz;
}

/* Gets the list of non-empty cells that are at distance k in the X, Y or Z
 * direction from the center cell with grid coordinates cell_gr_coord. The
 * list is stored in dlists->list[k] and dlists->n_dists is updated to reflect
 * the number of calculated distances. The list of empty cells is obtained
 * from the list of faces in each cell, fic. The size of the cell grid is
 * given by grid_sz. The distance between two cells is the minimum distance
 * between points in each cell. For distance zero the center cell is also
 * included in the list. */
static void get_cells_at_distance(struct dist_cell_lists *dlists,
                                  struct size3d cell_gr_coord,
                                  struct size3d grid_sz, int k,
                                  const struct t_in_cell_list *fic)
{
  int max_n_cells;
  int cell_idx;
  int cell_idx1,cell_idx2;
  int cell_stride_z;
  ec_bitmap_t *fic_empty_cell;
  int *cell_list;
  int *cur_cell;
  int cll;
  int m,n,o;
  int m1,m2,n1,n2,o1,o2;
  int min_m,max_m,min_n,max_n,min_o,max_o;
  int d;
  int tmp;

  assert(k == 0 || dlists->n_dists <= k);

  /* Initialize */
  cell_stride_z = grid_sz.y*grid_sz.x;
  fic_empty_cell = fic->empty_cell;

  /* Expand storage for distance cell list */
  dlists->list = xa_realloc(dlists->list,(k+1)*sizeof(*(dlists->list)));
  if (k > dlists->n_dists){
    /* set to NULL new elements that will not be filled */
    memset(dlists->list+dlists->n_dists,0,
           (k-dlists->n_dists)*sizeof(*(dlists->list)));
  }
  dlists->n_dists = k+1;

  /* Get the cells that are at distance k in the X, Y or Z direction from the
   * center cell. For the zero distance we also include the center cell. */
  max_n_cells = 6*(2*k+1)*(2*k+1)+12*(2*k+1)+8;
  if (k == 0) max_n_cells += 1; /* add center cell */
  cell_list = xa_malloc(max_n_cells*sizeof(*cell_list));
  cur_cell = cell_list;
  if (k == 0) { /* add center cell */
    cell_idx = cell_gr_coord.x+cell_gr_coord.y*grid_sz.x+
      cell_gr_coord.z*cell_stride_z;
    if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx)) {
      *(cur_cell++) = cell_idx;
    }
  }
  /* Try to put the cells in the order of increasing distances (minimizes
   * number of cells and triangles to test). Doing a full ordering is too
   * slow, we just do what we can fast. */
  d = k+1; /* max displacement */
  min_o = max(cell_gr_coord.z-d+1,0);
  max_o = min(cell_gr_coord.z+d-1,grid_sz.z-1);
  min_n = max(cell_gr_coord.y-d+1,0);
  max_n = min(cell_gr_coord.y+d-1,grid_sz.y-1);
  m1 = cell_gr_coord.x-d;
  m2 = cell_gr_coord.x+d;
  if (m1 >= 0) {
    if (m2 < grid_sz.x) { /* left + right layer */
      for (o = min_o; o <= max_o; o++) {
        for (n = min_n; n <= max_n; n++) {
          tmp = n*grid_sz.x+o*cell_stride_z;
          cell_idx1 = m1+tmp;
          cell_idx2 = m2+tmp;
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx1)) {
            *(cur_cell++) = cell_idx1;
          }
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx2)) {
            *(cur_cell++) = cell_idx2;
          }
        }
      }
    } else { /* left layer */
      for (o = min_o; o <= max_o; o++) {
        for (n = min_n; n <= max_n; n++) {
          cell_idx = m1+n*grid_sz.x+o*cell_stride_z;
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx)) {
            *(cur_cell++) = cell_idx;
          }
        }
      }
    }
  } else {
    if (m2 < grid_sz.x) { /* right layer */
      for (o = min_o; o <= max_o; o++) {
        for (n = min_n; n <= max_n; n++) {
          cell_idx = m2+n*grid_sz.x+o*cell_stride_z;
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx)) {
            *(cur_cell++) = cell_idx;
          }
        }
      }
    }
  }
  min_m = max(cell_gr_coord.x-d,0);
  max_m = min(cell_gr_coord.x+d,grid_sz.x-1);
  n1 = cell_gr_coord.y-d;
  n2 = cell_gr_coord.y+d;
  if (n1 >= 0) {
    if (n2 < grid_sz.y) { /* back + front layers */
      for (o = min_o; o <= max_o; o++) {
        for (m = min_m; m <= max_m; m++) {
          tmp = m+o*cell_stride_z;
          cell_idx1 = tmp+n1*grid_sz.x;
          cell_idx2 = tmp+n2*grid_sz.x;
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx1)) {
            *(cur_cell++) = cell_idx1;
          }
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx2)) {
            *(cur_cell++) = cell_idx2;
          }
        }
      }
    } else { /* back layer */
      for (o = min_o; o <= max_o; o++) {
        for (m = min_m; m <= max_m; m++) {
          cell_idx = m+n1*grid_sz.x+o*cell_stride_z;
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx)) {
            *(cur_cell++) = cell_idx;
          }
        }
      }
    }
  } else {
    if (n2 < grid_sz.y) { /* front layer */
      for (o = min_o; o <= max_o; o++) {
        for (m = min_m; m <= max_m; m++) {
          cell_idx = m+n2*grid_sz.x+o*cell_stride_z;
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx)) {
            *(cur_cell++) = cell_idx;
          }
        }
      }
    }
  }
  min_n = max(cell_gr_coord.y-d,0);
  max_n = min(cell_gr_coord.y+d,grid_sz.y-1);
  o1 = cell_gr_coord.z-d;
  o2 = cell_gr_coord.z+d;
  if (o1 >= 0) {
    if (o2 < grid_sz.z) { /* bottom + top layers */
      for (n = min_n; n <= max_n; n++) {
        for (m = min_m; m <= max_m; m++) {
          tmp = m+n*grid_sz.x;
          cell_idx1 = tmp+o1*cell_stride_z;
          cell_idx2 = tmp+o2*cell_stride_z;
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx1)) {
            *(cur_cell++) = cell_idx1;
          }
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx2)) {
            *(cur_cell++) = cell_idx2;
          }
        }
      }
    } else { /* bottom */
      for (n = min_n; n <= max_n; n++) {
        for (m = min_m; m <= max_m; m++) {
          cell_idx = m+n*grid_sz.x+o1*cell_stride_z;
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx)) {
            *(cur_cell++) = cell_idx;
          }
        }
      }
    }
  } else {
    if (o2 < grid_sz.z) { /* top layer */
      for (n = min_n; n <= max_n; n++) {
        for (m = min_m; m <= max_m; m++) {
          cell_idx = m+n*grid_sz.x+o2*cell_stride_z;
          if (!EC_BITMAP_TEST_BIT(fic_empty_cell,cell_idx)) {
            *(cur_cell++) = cell_idx;
          }
        }
      }
    }
  }
  /* Store resulting cell list */
  cll = cur_cell-cell_list;
  if (cll != 0) {
    dlists->list[k].cell = xa_realloc(cell_list,cll*sizeof(*cell_list));
    dlists->list[k].n_cells = cll;
  } else {
    dlists->list[k].cell = NULL;
    dlists->list[k].n_cells = 0;
    free(cell_list);
  }
}

/* --------------------------------------------------------------------------*
 *                            Local functions                                *
 * --------------------------------------------------------------------------*/

/* Initializes the triangle '*t' using the '*a' '*b' and '*c' vertices and
 * calculates all the relative fields of the struct. */
static void init_triangle(const vertex *a, const vertex *b, const vertex *c,
                          struct triangle_info *t)
{
  vertex ab,ac,bc;
  double ab_len_sqr,ac_len_sqr,bc_len_sqr;
  double n_len;
  double ca_ab;
  double height_sqr;
  int is_point;

  /* Get the vertices in the proper ordering (the orientation is not
   * changed). AB should be the longest side. */
  substract_v(b,a,&ab);
  substract_v(c,a,&ac);
  substract_v(c,b,&bc);
  ab_len_sqr = norm2_v(&ab);
  ac_len_sqr = norm2_v(&ac);
  bc_len_sqr = norm2_v(&bc);
  if (ab_len_sqr <= ac_len_sqr) {
    if (ac_len_sqr <= bc_len_sqr) { /* BC longest side => A to C */
      assert(bc_len_sqr >= ac_len_sqr && bc_len_sqr >= ab_len_sqr);
      t->c = *a;
      t->a = *b;
      t->b = *c;
      t->ab = bc;
      t->ca = ab;
      t->cb = ac;
      t->ab_len_sqr = bc_len_sqr;
      t->ca_len_sqr = ab_len_sqr;
      t->cb_len_sqr = ac_len_sqr;
    } else { /* AC longest side => B to C */
      assert(ac_len_sqr >= bc_len_sqr && ac_len_sqr >= ab_len_sqr);
      t->b = *a;
      t->c = *b;
      t->a = *c;
      neg_v(&ac,&(t->ab));
      t->ca = bc;
      neg_v(&ab,&(t->cb));
      t->ab_len_sqr = ac_len_sqr;
      t->ca_len_sqr = bc_len_sqr;
      t->cb_len_sqr = ab_len_sqr;
    }
  } else {
    if (ab_len_sqr <= bc_len_sqr) { /* BC longest side => A to C */
      assert(bc_len_sqr >= ac_len_sqr && bc_len_sqr >= ab_len_sqr);
      t->c = *a;
      t->a = *b;
      t->b = *c;
      t->ab = bc;
      t->ca = ab;
      t->cb = ac;
      t->ab_len_sqr = bc_len_sqr;
      t->ca_len_sqr = ab_len_sqr;
      t->cb_len_sqr = ac_len_sqr;
    } else { /* AB longest side => C remains C */
      assert(ab_len_sqr >= ac_len_sqr && ab_len_sqr >= bc_len_sqr);
      t->a = *a;
      t->b = *b;
      t->c = *c;
      t->ab = ab;
      neg_v(&ac,&(t->ca));
      neg_v(&bc,&(t->cb));
      t->ab_len_sqr = ab_len_sqr;
      t->ca_len_sqr = ac_len_sqr;
      t->cb_len_sqr = bc_len_sqr;
    }
  }
  if (t->ab_len_sqr < DBL_MIN*DMARGIN) {
    is_point = 1; /* ABC coincident => degenerates to a point */
    t->ab.x = 0;
    t->ab.y = 0;
    t->ab.z = 0;
    t->cb = t->ab;
    t->ca = t->ab;
    t->ab_len_sqr = 0;
    t->ca_len_sqr = 0;
    t->cb_len_sqr = 0;
  } else {
    is_point = 0;
  }
  /* Get side lengths */
  t->ab_1_len_sqr = 1/t->ab_len_sqr;
  t->ca_1_len_sqr = 1/t->ca_len_sqr;
  t->cb_1_len_sqr = 1/t->cb_len_sqr;
  /* Get the triangle normal (normalized) */
  crossprod_v(&(t->ca),&(t->ab),&(t->normal));
  n_len = norm_v(&(t->normal));
  if (n_len < DBL_MIN*DMARGIN) {
    t->normal.x = 0;
    t->normal.y = 0;
    t->normal.z = 0;
  } else {
    prod_v(1/n_len,&(t->normal),&(t->normal));
  }
  /* Get planes trough sides */
  crossprod_v(&(t->ab),&(t->normal),&(t->nhsab));
  crossprod_v(&(t->normal),&(t->cb),&(t->nhsbc));
  crossprod_v(&(t->ca),&(t->normal),&(t->nhsca));
  /* Get constants for plane equations */
  t->chsab = scalprod_v(&(t->a),&(t->nhsab));
  t->chsca = scalprod_v(&(t->a),&(t->nhsca));
  t->chsbc = scalprod_v(&(t->b),&(t->nhsbc));
  /* Miscellaneous fields */
  t->wide_at_c = (t->ab_len_sqr > t->ca_len_sqr+t->cb_len_sqr);
  t->a_n = scalprod_v(&(t->a),&(t->normal));
  /* Get surface area */
  if (is_point) {
    t->s_area = 0;
  } else {
    ca_ab = scalprod_v(&(t->ca),&(t->ab));
    height_sqr = t->ca_len_sqr-ca_ab*ca_ab*t->ab_1_len_sqr;
    if (height_sqr < 0) height_sqr = 0; /* avoid rounding problems */
    t->s_area = sqrt(t->ab_len_sqr*height_sqr)*0.5;
  }
}

/* Compute the square of the distance between point 'p' and triangle 't' in 3D
 * space. The distance from a point p to a triangle is defined as the
 * Euclidean distance from p to the closest point in the triangle. */
static double dist_sqr_pt_triag(const struct triangle_info *t, const vertex *p)
{
  double dpp;             /* (signed) distance point to ABC plane */
  double ap_ab,cp_cb,cp_ca; /* scalar products */
  vertex ap,cp;           /* Point to point vectors */
  double dmin_sqr;        /* minimum distance squared */

  /* NOTE: If the triangle has a wide angle (i.e. angle larger than 90
   * degrees) it is the angle at the C vertex (never at A or B). */

  /* We get the distance from point P to triangle ABC by first testing on
   * which side of the planes (hsab, hsbc and hsca) perpendicular to the ABC
   * plane trough AB, BC and CA is P. If P is towards the triangle center from
   * all three planes, the projection of P on the ABC plane is interior to ABC
   * and thus the distance to ABC is the distance to the ABC plane. If P is
   * towards the triangle exterior from the plane hsab, then the distance from
   * P to ABC is the distance to the AB segment. Otherwise if P is towards the
   * triangle exterior from the plane hsbc the distance from P to ABC is the
   * minimum of the distance to the BC and CA segments (only BC is the angle
   * at C is not wide). Otherwise P is towards the triangle exterior from the
   * plane hsac and the distance from P to ABC is the distance to the CA
   * segment. */

  /* NOTE: if the triangle is degenerate t->npab and t->cpab are identically
   * (0,0,0), so first 'if' test is true (other 'if's never get degenerated
   * triangles). Furthermore, if the AB side is degenerate (that is the
   * triangle degenerates to a point since AB is longest side) t->ab is
   * identically (0,0,0) also and the distance to A is calculated. */
  if (scalprod_v(p,&(t->nhsab)) >= t->chsab) {
    /* P in the exterior side of hsab plane => closest to AB */
    substract_v(p,&(t->a),&ap);
    ap_ab = scalprod_v(&ap,&(t->ab));
    if(ap_ab > 0) {
      if (ap_ab < t->ab_len_sqr) { /* projection of P on AB is in AB */
        dmin_sqr = norm2_v(&ap) - (ap_ab*ap_ab)*t->ab_1_len_sqr;
        if (dmin_sqr < 0) dmin_sqr = 0; /* correct rounding problems */
        return dmin_sqr;
      } else { /* B is closer */
        return dist2_v(p,&(t->b));
      }
    } else { /* A is closer */
      return norm2_v(&ap);
    }
  } else if (scalprod_v(p,&(t->nhsbc)) >= t->chsbc) {
    /* P in the exterior side of hsbc plane => closest to BC or AC */
    substract_v(p,&(t->c),&cp);
    cp_cb = scalprod_v(&cp,&(t->cb));
    if(cp_cb > 0) {
      if (cp_cb < t->cb_len_sqr) { /* projection of P on BC is in BC */
        dmin_sqr = norm2_v(&cp) - (cp_cb*cp_cb)*t->cb_1_len_sqr;
        if (dmin_sqr < 0) dmin_sqr = 0; /* correct rounding problems */
        return dmin_sqr;
      } else { /* B is closer */
        return dist2_v(p,&(t->b));
      }
    } else if (!t->wide_at_c) { /* C is closer */
      return norm2_v(&cp);
    } else { /* AC is closer */
      cp_ca = scalprod_v(&cp,&(t->ca));
      if(cp_ca > 0) {
        if (cp_ca < t->ca_len_sqr) { /* projection of P on AC is in AC */
          dmin_sqr = norm2_v(&cp) - (cp_ca*cp_ca)*t->ca_1_len_sqr;
          if (dmin_sqr < 0) dmin_sqr = 0; /* correct rounding problems */
          return dmin_sqr;
        } else { /* A is closer */
          return dist2_v(p,&(t->a));
        }
      } else { /* C is closer */
        return norm2_v(&cp);
      }
    }
  } else if (scalprod_v(p,&(t->nhsca)) >= t->chsca) {
    /* P in the exterior side of hsca plane => closest to AC */
    substract_v(p,&(t->c),&cp);
    cp_ca = scalprod_v(&cp,&(t->ca));
    if(cp_ca > 0) {
      if (cp_ca < t->ca_len_sqr) { /* projection of P on AC is in AC */
        dmin_sqr = norm2_v(&cp) - (cp_ca*cp_ca)*t->ca_1_len_sqr;
        if (dmin_sqr < 0) dmin_sqr = 0; /* correct rounding problems */
        return dmin_sqr;
      } else { /* A is closer */
        return dist2_v(p,&(t->a));
      }
    } else { /* C is closer */
      return norm2_v(&cp);
    }
  } else { /* P projects into triangle */
    dpp = scalprod_v(p,&(t->normal))-t->a_n;
    return dpp*dpp;
  }
}

/* Calculates the square of the distance between a point p in cell
 * (gr_x,gr_y,gr_z) and cell cell_idx (liner index). The coordinates of p are
 * relative to the minimum X,Y,Z coordinates of the bounding box from where
 * the cell grid is derived. All the cells are cubic, with a side of length
 * cell_sz. If the point p is in the cell (m,n,o) the distance is zero. The
 * number of cells in the grid along X is given by grid_sz_x, and the
 * separation between adjacent cells along Z is given by cell_stride_z. */
static INLINE double dist_sqr_pt_cell(const vertex *p, int gr_x, int gr_y,
                                      int gr_z, int cell_idx, int grid_sz_x,
                                      int cell_stride_z, double cell_sz)
{
  double d2,tmp;
  int m,n,o,tmpi;

  /* Get 3D indices of cell cell_idx */
  o = cell_idx/cell_stride_z;
  tmpi = cell_idx%cell_stride_z;
  n = tmpi/grid_sz_x;
  m = tmpi%grid_sz_x;
  /* Calculate distance */
  d2 = 0;
  if (gr_x != m) { /* if not on same cell x wise */
    tmp = (m > gr_x) ? m*cell_sz-p->x : p->x-(m+1)*cell_sz;
    d2 += tmp*tmp;
  }
  if (gr_y != n) { /* if not on same cell y wise */
    tmp = (n > gr_y) ? n*cell_sz-p->y : p->y-(n+1)*cell_sz;
    d2 += tmp*tmp;
  }
  if (gr_z != o) { /* if not on same cell z wise */
    tmp = (o > gr_z) ? o*cell_sz-p->z : p->z-(o+1)*cell_sz;
    d2 += tmp*tmp;
  }
  return d2;
}

/* Convert the triangular model m to a triangle list (without connectivity
 * information) with the associated information. All the information about the
 * triangles (i.e. fields of struct triangle_info) is computed. */
static struct triangle_list* model_to_triangle_list(const model *m)
{
  int i,n;
  struct triangle_list *tl;
  struct triangle_info *triags;
  face *face_i;

  /* Initialize and allocate storage */
  n = m->num_faces;
  tl = xa_malloc(sizeof(*tl));
  tl->n_triangles = n;
  triags = xa_malloc(sizeof(*tl->triangles)*n);
  tl->triangles = triags;
  tl->area = 0;

  /* Convert triangles and update global data */
  for (i=0; i<n; i++) {
    face_i = &(m->faces[i]);
    init_triangle(&(m->vertices[face_i->f0]),&(m->vertices[face_i->f1]),
                  &(m->vertices[face_i->f2]),&(triags[i]));
    tl->area += triags[i].s_area;
  }

  return tl;
}

/* Calculates the statistics of the error samples in tse. For each triangle
 * formed by neighboring samples the error at the vertices is averaged to
 * obtain a single error for the sample triangle. The overall mean error is
 * obtained by calculating the mean of the errors of the sample triangles. The
 * other statistics are obtained analogously. Note that all sample triangles
 * have exactly the same area, and thus the calculation is independent of the
 * triangle shape. */
static void error_stat_triag(const struct triag_sample_error *tse,
                             struct face_error *fe)
{
  int n,i,j,imax,jmax;
  double err_local;
  double err_a,err_b,err_c;
  double err_min, err_max, err_tot, err_sqr_tot;
  double **s_err;

  /* NOTE: In a triangle with values at the vertex e1, e2 and e3 and using
   * linear interpolation to obtain the values within the triangle, the mean
   * value (i.e. integral of the value divided by the surface) is
   * (e1+e2+e3)/3; and the squared mean value (i.e. integral of the squared
   * value divided by the surface) is (e1^2+e2^2+e3^2+e1*e2+e2*e3+e1*e3)/6. */
  err_min = DBL_MAX;
  err_max = 0;
  err_tot = 0;
  err_sqr_tot = 0;
  n = tse->n_samples;
  s_err = tse->err;
  /* Do sample triangles for which the point (i,j) is closer to the point
   * (0,0) than the side of the sample triangle opposite (i,j). There are
   * (n-1)*n/2 of these. */
  for (i=0, imax=n-1; i<imax; i++) {
    for (j=0, jmax=imax-i; j<jmax; j++) {
      err_a = s_err[i][j];
      err_b = s_err[i][j+1];
      err_c = s_err[i+1][j];
      err_tot += err_a+err_b+err_c;
      err_sqr_tot += err_a*(err_a+err_b+err_c)+err_b*(err_b+err_c)+err_c*err_c;
    }
  }
  /* Do the other triangles. There are (n-2)*(n-1)/2 of these. */
  for (i=1; i<n; i++) {
    for (j=1, jmax=n-i; j<jmax; j++) {
      err_a = s_err[i-1][j];
      err_b = s_err[i][j-1];
      err_c = s_err[i][j];
      err_tot += err_a+err_b+err_c;
      err_sqr_tot += err_a*(err_a+err_b+err_c)+err_b*(err_b+err_c)+err_c*err_c;
    }
  }
  /* Get min max */
  for (i=0; i<tse->n_samples_tot; i++) {
    err_local = tse->err_lin[i];
    if (err_min > err_local) err_min = err_local;
    if (err_max < err_local) err_max = err_local;
  }
  /* Finalize error measures */
  fe->min_error = err_min;
  fe->max_error = err_max;
  if (n != 1) { /* normal case */
    fe->mean_error = err_tot/(((n-1)*n/2+(n-2)*(n-1)/2)*3);
    fe->mean_sqr_error = err_sqr_tot/(((n-1)*n/2+(n-2)*(n-1)/2)*6);
  } else { /* special case */
    fe->mean_error = tse->err_lin[0];
    fe->mean_sqr_error = tse->err_lin[0]*tse->err_lin[0];
  }
}

/* Samples a triangle (a,b,c) using n samples in each direction. The sample
 * points are returned in the sample_list s. The dynamic array 's->sample' is
 * realloc'ed to the correct size (if no storage has been previously allocated
 * it should be NULL). The total number of samples is n*(n+1)/2. The order for
 * samples (i,j) in s->sample is all samples for i equal 0 and j from 0 to n-1,
 * followed by all samples for i equal 1 and j from 0 to n-2, and so on, where
 * i and j are the sampling indices along the ab and ac sides,
 * respectively. As a special case, if n equals 1, the triangle middle point
 * is used as the sample. */
static void sample_triangle(const vertex *a, const vertex *b, const vertex *c,
                            int n, struct sample_list* s)
{
  vertex u,v;     /* basis parametrization vectors */
  vertex a_cache; /* local (on stack) copy of a for faster access */
  int i,j,maxj,k; /* counters and limits */

  /* initialize */
  a_cache = *a;
  s->n_samples = n*(n+1)/2;
  s->sample = xa_realloc(s->sample,sizeof(vertex)*s->n_samples);
  /* get basis vectors */
  substract_v(b,a,&u);
  substract_v(c,a,&v);
  if (n != 1) { /* normal case */
    prod_v(1/(double)(n-1),&u,&u);
    prod_v(1/(double)(n-1),&v,&v);
    /* Sample triangle */
    for (k = 0, i = 0; i < n; i++) {
      for (j = 0, maxj = n-i; j < maxj; j++) {
        s->sample[k].x = a_cache.x+i*u.x+j*v.x;
        s->sample[k].y = a_cache.y+i*u.y+j*v.y;
        s->sample[k++].z = a_cache.z+i*u.z+j*v.z;
      }
    }
  } else { /* special case, use triangle middle point */
    s->sample[0].x = a_cache.x+0.5*u.x+0.5*v.x;
    s->sample[0].y = a_cache.y+0.5*u.y+0.5*v.y;
    s->sample[0].z = a_cache.z+0.5*u.z+0.5*v.z;
  }
}

/* Given a triangle list tl, returns the list of triangle indices that
 * intersect a cell, for each cell in the grid. The size of the grid is given
 * by grid_sz, the side length of the cubic cells by cell_sz and the minimum
 * coordinates of the bounding box (i.e. origin) of the grid by bbox_min. The
 * returned struct, its arrays and subarrays are malloc'ed independently. */
static struct t_in_cell_list *triangles_in_cells(const struct triangle_list *tl,
                                                 struct size3d grid_sz,
                                                 double cell_sz,
                                                 vertex bbox_min)
{
  struct t_in_cell_list *lst; /* The list to return */
  struct sample_list sl;      /* samples from a triangle */
  int **tab;                  /* Table containing the indices of intersecting
                               * triangles for each cell. */
  int *nt;                    /* Array with the number of intersecting
                               * triangles found so far for each cell */
  ec_bitmap_t *ecb;           /* The empty cell bitmap */
  int cell_idx,cell_idx_prev; /* linear (1D) cell indices */
  int cell_stride_z;          /* spacement for Z index in 3D addressing of
                               * cell list */
  int i,j,h,imax;             /* counters and loop limits */
  int m_a,n_a,o_a,m_b,n_b,o_b,m_c,n_c,o_c; /* 3D cell indices for vertices */
  int tmpi,max_cell_dist;     /* maximum cell distance along any axis */
  int n_samples;              /* number of samples to use for triangles */
  int m,n,o;                  /* 3D cell indices for samples */
  int *c_buf;                 /* temp storage for cell list */
  int c_buf_sz;               /* the size of c_buf */

  /* Initialize */
  cell_stride_z = grid_sz.x*grid_sz.y;
  c_buf = NULL;
  c_buf_sz = 0;
  sl.sample = NULL;
  lst = xa_malloc(sizeof(*lst));
  nt = xa_calloc(grid_sz.x*grid_sz.y*grid_sz.z,sizeof(*nt));
  tab = xa_calloc(grid_sz.x*grid_sz.y*grid_sz.z,sizeof(*tab));
  ecb = xa_calloc((grid_sz.x*grid_sz.y*grid_sz.z+EC_BITMAP_T_BITS-1)/
                  EC_BITMAP_T_BITS,EC_BITMAP_T_SZ);
  lst->triag_idx = tab;
  lst->n_cells = grid_sz.x*grid_sz.y*grid_sz.z;
  lst->empty_cell = ecb;

  /* Get intersecting cells for each triangle */
  for (i=0, imax=tl->n_triangles; i<imax;i++) {
    /* Get the cells in which the triangle vertices are. For non-negative
     * values, cast to int is equivalent to floor and probably faster (here
     * negative values can not happen since bounding box is obtained from the
     * vertices in tl). */
    m_a = (int)((tl->triangles[i].a.x-bbox_min.x)/cell_sz);
    n_a = (int)((tl->triangles[i].a.y-bbox_min.y)/cell_sz);
    o_a = (int)((tl->triangles[i].a.z-bbox_min.z)/cell_sz);
    m_b = (int)((tl->triangles[i].b.x-bbox_min.x)/cell_sz);
    n_b = (int)((tl->triangles[i].b.y-bbox_min.y)/cell_sz);
    o_b = (int)((tl->triangles[i].b.z-bbox_min.z)/cell_sz);
    m_c = (int)((tl->triangles[i].c.x-bbox_min.x)/cell_sz);
    n_c = (int)((tl->triangles[i].c.y-bbox_min.y)/cell_sz);
    o_c = (int)((tl->triangles[i].c.z-bbox_min.z)/cell_sz);

    if (m_a == m_b && m_a == m_c && n_a == n_b && n_a == n_c &&
        o_a == o_b && o_a == o_c) {
      /* The ABC triangle fits entirely into one cell => fast case */
      cell_idx = m_a+n_a*grid_sz.x+o_a*cell_stride_z;
      tab[cell_idx] = xa_realloc(tab[cell_idx],(nt[cell_idx]+2)*sizeof(**tab));
      tab[cell_idx][nt[cell_idx]++] = i;
      continue;
    }

    /* Triangle does not fit in one cell, how many cells does the triangle
     * span ? */
    max_cell_dist = abs(m_a-m_b);
    if ((tmpi = abs(m_a-m_c)) > max_cell_dist) max_cell_dist = tmpi;
    if ((tmpi = abs(m_b-m_c)) > max_cell_dist) max_cell_dist = tmpi;
    if ((tmpi = abs(n_a-n_b)) > max_cell_dist) max_cell_dist = tmpi;
    if ((tmpi = abs(n_a-n_c)) > max_cell_dist) max_cell_dist = tmpi;
    if ((tmpi = abs(n_b-n_c)) > max_cell_dist) max_cell_dist = tmpi;
    if ((tmpi = abs(o_a-o_b)) > max_cell_dist) max_cell_dist = tmpi;
    if ((tmpi = abs(o_a-o_c)) > max_cell_dist) max_cell_dist = tmpi;
    if ((tmpi = abs(o_b-o_c)) > max_cell_dist) max_cell_dist = tmpi;
    /* Sample the triangle so as to have twice the samples in any direction
     * than the number of cells spanned in that direction. */
    n_samples = 2*(max_cell_dist+1);
    sample_triangle(&(tl->triangles[i].a),&(tl->triangles[i].b),
                    &(tl->triangles[i].c),n_samples,&sl);
    /* Get the intersecting cells from the samples */
    cell_idx_prev = -1;
    h = 0;
    for(j=0;j<sl.n_samples;j++){
      /* Get cell in which the sample is. Due to rounding in the triangle
       * sampling process we check the indices to be within bounds. As above,
       * we can use cast to int instead of floor (probably faster) */
      m=(int)((sl.sample[j].x-bbox_min.x)/cell_sz);
      if(m >= grid_sz.x) {
        m = grid_sz.x - 1;
      } else if (m < 0) {
        m = 0;
      }
      n=(int)((sl.sample[j].y-bbox_min.y)/cell_sz);
      if (n >= grid_sz.y) {
        n = grid_sz.y - 1;
      } else if (n < 0) {
        n = 0;
      }
      o=(int)((sl.sample[j].z-bbox_min.z)/cell_sz);
      if (o >= grid_sz.z) {
        o = grid_sz.z - 1;
      } else if (o < 0) {
        o = 0;
      }

      /* Include cell index in list only if not the same as previous one
       * (avoid too many duplicates). */
      cell_idx = m + n*grid_sz.x + o*cell_stride_z;
      if (cell_idx != cell_idx_prev) {
        if (c_buf_sz <= h) {
          c_buf = xa_realloc(c_buf, (h+1)*sizeof(*c_buf));
          c_buf_sz++;
        }
        c_buf[h++] = cell_idx;
        cell_idx_prev = cell_idx;
      }
    }

    /* Include triangle in intersecting cell lists, without duplicate. */
    for (j=0; j<h; j++) {
      cell_idx = c_buf[j];
      if (nt[cell_idx] == 0 || tab[cell_idx][nt[cell_idx]-1] != i) {
        tab[cell_idx] = xa_realloc(tab[cell_idx],
                                   (nt[cell_idx]+2)*sizeof(**tab));
        tab[cell_idx][nt[cell_idx]++] = i;
      }
    }
  }

  /* Terminate lists with -1 and set empty cell bitmap */
  for(i=0, imax=grid_sz.x*grid_sz.y*grid_sz.z; i<imax; i++){
    if (nt[i] == 0) { /* mark empty cell in bitmap */
      EC_BITMAP_SET_BIT(ecb,i);
    } else {
      tab[i][nt[i]] = -1;
    }
  }

  free(nt);
  free(sl.sample);
  free(c_buf);
  return lst;
}

/* Returns the distance from point p to the surface defined by the triangle
 * list tl. The distance from a point to a surface is defined as the distance
 * from a point to the closets point on the surface. To speed up the search
 * for the closest triangle in the surface the bounding box of the model is
 * subdivided in cubic cells. The list of triangles that intersect each cell
 * is given by fic, as returned by the triangles_in_cells() function. The side
 * of the cubic cells is of length cell_sz, and there are
 * (grid_sz.x,grid_sz.y,grid_sz.z) cells in teh X,Y,Z directions. Cell (0,0,0)
 * starts at bbox_min, which is the minimum coordinates of the (axis aligned)
 * bounding box on which the grid is placed. If DO_DIST_PT_SURF_STATS is
 * defined at compile time, the statistics stats are updated (no reset to zero
 * occurs, the counters are increased). The list of cells distant of k cells
 * in the X, Y or Z direction, for each cell, is cached in dcl, which must be
 * an array of length grid_sz.x*grid_sz.y*grid_sz.z and must be initialized to
 * all zero on the first call to this function. The distance obtained from a
 * previous point *prev_p is prev_d (it is used to minimize the work). For the
 * first call set prev_d as zero. */
static double dist_pt_surf(vertex p, const struct triangle_list *tl,
                           const struct t_in_cell_list *fic,
#ifdef DO_DIST_PT_SURF_STATS
                           struct dist_pt_surf_stats *stats,
#endif
                           struct size3d grid_sz, double cell_sz,
                           vertex bbox_min, struct dist_cell_lists *dcl,
                           const vertex *prev_p, double prev_d)
{
  vertex p_rel;         /* coordinates of p relative to bbox_min */
  struct size3d grid_coord; /* coordinates of cell in which p is */
  int k;                /* cell index distance of current scan */
  int kmax;             /* maximum limit for k (avoid infinite loops) */
  int cell_idx;         /* linear cell index */
  double dmin_sqr;      /* minimum distance squared */
  double dist_sqr;      /* current distance squared */
  double cell_sz_sqr;   /* cubic cell side length squared */
  int t_idx;            /* triangle index in triangle list */
  int cell_stride_z;    /* spacement for Z index in 3D addressing of cell
                         * list */
  int *cur_cell_tl;     /* list of triangles intersecting the current cell */
  struct triangle_info *triags; /* local pointer to triangle array */
  int *cur_cell;        /* current cell in the list of cells to scan for the
                         * current k */
  int *end_cell;        /* one past the last cell in the current cell list */
  ec_bitmap_t *fic_empty_cell; /* stack copy of fic->empty_cell (faster) */
  int **fic_triag_idx;  /* stack copy of fic->triag_idx (faster) */
  double dmin;          /* minimum possible distance to any triangle */

  /* NOTE: tests have shown it is faster to scan each triangle, even
   * repeteadly, than to track which triangles have been scanned (too much
   * time spent initializing tracking info to zero). */

  /* Initialize */
  cell_stride_z = grid_sz.y*grid_sz.x;
  triags = tl->triangles;
  fic_empty_cell = fic->empty_cell;
  fic_triag_idx = fic->triag_idx;

  /* Get relative coordinates of point */
  substract_v(&p,&bbox_min,&p_rel);
  /* Get the cell coordinates of where point is. Since the bounding box bbox
   * is that of the model 2, the grid coordinates can be out of bounds (in
   * which case we limit them) */
  grid_coord.x = floor(p_rel.x/cell_sz);
  if (grid_coord.x < 0) {
    grid_coord.x = 0;
  } else if (grid_coord.x >= grid_sz.x) {
    grid_coord.x = grid_sz.x-1;
  }
  grid_coord.y = floor(p_rel.y/cell_sz);
  if (grid_coord.y < 0) {
    grid_coord.y = 0;
  } else if (grid_coord.y >= grid_sz.y) {
    grid_coord.y = grid_sz.y-1;
  }
  grid_coord.z = floor(p_rel.z/cell_sz);
  if (grid_coord.z < 0) {
    grid_coord.z = 0;
  } else if (grid_coord.z >= grid_sz.z) {
    grid_coord.z = grid_sz.z-1;
  }

  /* Determine starting k, based on previous point (which is typically close
   * to current point) and its distance to closest triangle */
  dmin = prev_d-dist_v(&p,prev_p);
  k = floor(dmin*SQRT_1_3/cell_sz)-2;
  if (k <0) k = 0;

  /* Scan cells, at sequentially increasing index distance k */
  kmax = max3(grid_sz.x,grid_sz.y,grid_sz.z);
  if (k >= kmax) k = kmax-1;
  dmin_sqr = DBL_MAX;
  cell_sz_sqr = cell_sz*cell_sz;
  do {
    /* Get the list of cells at distance k in X Y or Z direction, which has
     * not been previously tested. Only non-empty cells are included in the
     * list. */
    cell_idx = grid_coord.x+grid_coord.y*grid_sz.x+grid_coord.z*cell_stride_z;
    if (dcl[cell_idx].n_dists <= k || dcl[cell_idx].list == NULL) {
      get_cells_at_distance(&(dcl[cell_idx]),grid_coord,grid_sz,k,fic);
    }

    /* Scan each (non-empty) cell in the compiled list */
    for (cur_cell = dcl[cell_idx].list[k].cell,
           end_cell = cur_cell+dcl[cell_idx].list[k].n_cells;
         cur_cell<end_cell; cur_cell++) {
      cell_idx = *cur_cell;
      /* If minimum distance from point to cell is larger than already
       * found minimum distance we can skip all triangles in the cell */
#ifdef DO_DIST_PT_SURF_STATS
      stats->n_cell_scans++;
#endif
      if (dmin_sqr < dist_sqr_pt_cell(&p_rel,grid_coord.x,grid_coord.y,
                                      grid_coord.z,cell_idx,grid_sz.x,
                                      cell_stride_z,cell_sz)) {
        continue;
      }
      /* Scan all triangles (i.e. faces) in the cell */
#ifdef DO_DIST_PT_SURF_STATS
      stats->n_cell_t_scans++;
#endif
      cur_cell_tl = fic_triag_idx[cell_idx];
      t_idx = *(cur_cell_tl++);
      do { /* cell has always one triangle at least, so do loop is OK */
#ifdef DO_DIST_PT_SURF_STATS
        stats->n_triag_scans++;
#endif
        dist_sqr = dist_sqr_pt_triag(&triags[t_idx],&p);
        if (dist_sqr < dmin_sqr) {
          dmin_sqr = dist_sqr;
        }
      } while ((t_idx = *(cur_cell_tl++)) >= 0);
    }
    /* We loop until the minimum distance to any of the cells to come is
     * larger than the minimum distance to a face found so far; or until all
     * cells have been tested. */
    k++;
  } while (k < kmax && dmin_sqr >= k*k*cell_sz_sqr);
#ifdef DO_DIST_PT_SURF_STATS
  stats->sum_kmax += k-1;
#endif
  if (dmin_sqr >= DBL_MAX || dmin_sqr != dmin_sqr || dmin_sqr < 0) {
    /* Something is going wrong (probably NaNs, etc.). The x != x test is for
     * NaNs (if supported, otherwise always true) */
    fprintf(stderr,
            "ERROR: entered infinite loop! NaN or infinte value in model ?\n"
            "       (otherwise you have stumbled on a bug, please report)\n");
    exit(1);
  }

  return  sqrt(dmin_sqr);
}

/* --------------------------------------------------------------------------*
 *                          External functions                               *
 * --------------------------------------------------------------------------*/

/* See compute_error.h */
void dist_surf_surf(const model *m1, model *m2, double sampling_step,
                    struct face_error *fe_ptr[],
                    struct dist_surf_surf_stats *stats, int calc_normals,
                    int quiet)
{
  vertex bbox_min,bbox_max;   /* min and max of bounding box of m1 and m2 */
  struct triangle_list *tl2;  /* triangle list for m2 */
  struct t_in_cell_list *fic; /* list of faces intersecting each cell */
  struct sample_list ts;      /* list of sample from a triangle */
  struct triag_sample_error tse; /* the errors at the triangle samples */
  int n;                      /* sampling frequency for current triangle */
  int i,k,kmax;               /* counters and loop limits */
  double cell_sz;             /* side length of the cubic cells */
  struct size3d grid_sz;      /* number of cells in the X, Y and Z directions */
  struct face_error *fe;      /* The error metrics for each face of m1 */
  int report_step;            /* The step to update the progress report */
  struct dist_cell_lists *dcl;/* Cache for the list of non-empty cells at each
                               * distance, for each cell. */
  vertex prev_p;              /* previous point */
  double prev_d;              /* distance for previous point */
#ifdef DO_DIST_PT_SURF_STATS
  struct dist_pt_surf_stats dps_stats; /* Statistics */
#endif

  /* Initialize */
  memset(&ts,0,sizeof(ts));
  memset(&tse,0,sizeof(tse));
  report_step = m1->num_faces/(100/2); /* report every 2 % */
  if (report_step <= 0) report_step = 1;
  bbox_min.x = min(m1->bBox[0].x,m2->bBox[0].x);
  bbox_min.y = min(m1->bBox[0].y,m2->bBox[0].y);
  bbox_min.z = min(m1->bBox[0].z,m2->bBox[0].z);
  bbox_max.x = max(m1->bBox[1].x,m2->bBox[1].x);
  bbox_max.y = max(m1->bBox[1].y,m2->bBox[1].y);
  bbox_max.z = max(m1->bBox[1].z,m2->bBox[1].z);
  prev_p.x = 0;
  prev_p.y = 0;
  prev_p.z = 0;
  prev_d = 0;

  /* Get the triangle list from model 2 and determine the grid and cell size */
  tl2 = model_to_triangle_list(m2);
  cell_sz = get_cell_size(tl2,&bbox_min,&bbox_max,&grid_sz);
  dcl = xa_calloc(grid_sz.x*grid_sz.y*grid_sz.z,sizeof(*dcl));

  /* Get the list of triangles in each cell */
  fic = triangles_in_cells(tl2,grid_sz,cell_sz,bbox_min);

  /* Allocate storage for errors */
  *fe_ptr = xa_realloc(*fe_ptr,m1->num_faces*sizeof(**fe_ptr));
  fe = *fe_ptr;

  /* Initialize overall statistics */
  stats->m1_samples = 0;
  stats->m1_area = 0;
  stats->m2_area = tl2->area;
  stats->min_dist = DBL_MAX;
  stats->max_dist = 0;
  stats->mean_dist = 0;
  stats->rms_dist = 0;
  stats->cell_sz = cell_sz;
  stats->grid_sz = grid_sz;
#ifdef DO_DIST_PT_SURF_STATS
  memset(&dps_stats,0,sizeof(dps_stats));
#endif

  /* For each triangle in model 1, sample and calculate the error */
  if (!quiet) printf("Progress %2d %%",0);
  for (k=0, kmax=m1->num_faces; k<kmax; k++) {
    if (!quiet && k!=0 && k%report_step) {
      printf("\rProgress %2d %%",100*k/(kmax-1));
      fflush(stdout);
    }
    fe[k].face_area = tri_area(m1->vertices[m1->faces[k].f0],
                               m1->vertices[m1->faces[k].f1],
                               m1->vertices[m1->faces[k].f2]);
    n = get_sampling_freq(&(m1->vertices[m1->faces[k].f0]),
                          &(m1->vertices[m1->faces[k].f1]),
                          &(m1->vertices[m1->faces[k].f2]),sampling_step);
    stats->m1_samples += (n*(n+1))/2;
    realloc_triag_sample_error(&tse,n);
    sample_triangle(&(m1->vertices[m1->faces[k].f0]),
                    &(m1->vertices[m1->faces[k].f1]),
                    &(m1->vertices[m1->faces[k].f2]),n,&ts);
    for (i=0; i<tse.n_samples_tot; i++) {
      tse.err_lin[i] = dist_pt_surf(ts.sample[i],tl2,fic,
#ifdef DO_DIST_PT_SURF_STATS
                                    &dps_stats,
#endif
                                    grid_sz,cell_sz,bbox_min,dcl,
                                    &prev_p,prev_d);
      prev_p = ts.sample[i];
      prev_d = tse.err_lin[i];
    }
    error_stat_triag(&tse,&fe[k]);
    /* Update overall statistics */
    stats->m1_area += fe[k].face_area;
    if (fe[k].min_error < stats->min_dist) stats->min_dist = fe[k].min_error;
    if (fe[k].max_error > stats->max_dist) stats->max_dist = fe[k].max_error;
    stats->mean_dist += fe[k].mean_error*fe[k].face_area;
    stats->rms_dist += fe[k].mean_sqr_error*fe[k].face_area;
  }
  if (!quiet) printf("\r              \r"); /* Remove progress message */
#ifdef DO_DIST_PT_SURF_STATS
  if (!quiet) {
    printf("Average number of scanned non-empty cells per sample: %g\n",
           ((double)dps_stats.n_cell_scans)/stats->m1_samples);
    printf("Average number of cells per sample for which triangles are "
           "scanned: %g\n",
           ((double)dps_stats.n_cell_t_scans)/stats->m1_samples);
    printf("Average number of triangles scanned per sample: %g\n",
           ((double)dps_stats.n_triag_scans)/stats->m1_samples);
    printf("Average maximum cell to cell distance: %g\n",
           ((double)dps_stats.sum_kmax)/stats->m1_samples);
  }
#endif
  /* Finalize overall statistics */
  stats->mean_dist /= stats->m1_area;
  stats->rms_dist = sqrt(stats->rms_dist/stats->m1_area);

  /* Do normals for model 2 if requested and not yet present */
  if (calc_normals && m2->normals == NULL && m2->face_normals == NULL) {
    calc_normals_as_oriented_model(m2,tl2);
  }

  /* free temporary storage */
  free(tl2->triangles);
  free(tl2);
  for (k=0, kmax=fic->n_cells; k<kmax; k++) {
    free(fic->triag_idx[k]);
  }
  free(fic->triag_idx);
  free(fic->empty_cell);
  free(fic);
  for (k=0, kmax=grid_sz.x*grid_sz.y*grid_sz.z; k<kmax; k++) {
    if (dcl[k].list != NULL) {
      for (i=0; i<dcl[k].n_dists; i++) {
        free(dcl[k].list[i].cell);
      }
      free(dcl[k].list);
    }
  }
  free(dcl);
  free_triag_sample_error(&tse);
  free(ts.sample);
}

/* See compute_error.h */
void free_face_error(struct face_error *fe)
{
  free(fe);
}
