/* $Id: rawview_utils.c,v 1.4 2002/06/11 15:41:37 aspert Exp $ */
#include <3dutils.h>
#include <rawview.h>
#include <rawview_misc.h>

/* Colormap generation (taken from the MESH code) */
/* Transforms a hue (in degrees) to the RGB equivalent. The saturation and
 * value are taken as the maximum. 
 */
static void hue2rgb(float hue, float *r, float *g, float *b) {
  float p,n;
  int k;

  /* Get principal component of angle */
  hue -= 360*(float)floor(hue/360);
  /* Get section */
  hue /= 60;
  k = (int)floor(hue);
  if (k == 6) {
    k = 5;
  } else if (k < 0) {
    k = 0;
  }
  p = hue - k;
  n = 1 - p;

  /* Get RGB values based on section */
  switch (k) {
  case 0:
    *r = 1;
    *g = p;
    *b = 0;
    break;
  case 1:
    *r = n;
    *g = 1;
    *b = 0;
    break;
  case 2:
    *r = 0;
    *g = 1;
    *b = p;
    break;
  case 3:
    *r = 0;
    *g = n;
    *b = 1;
    break;
  case 4:
    *r = p;
    *g = 0;
    *b = 1;
    break;
  case 5:
    *r = 1;
    *g = 0;
    *b = n;
    break;
  default:
    abort(); /* should never get here */
  }
}

float** colormap_hsv(int len)
{
  float **cmap,step;
  int i;

  if (len <= 1) return NULL;

  cmap = malloc(len*sizeof(*cmap));
  *cmap = malloc(3*len*sizeof(**cmap));
  for (i=1; i<len; i++) {
    cmap[i] = cmap[i-1]+3;
  }

  step = 240/(float)(len-1);
  for (i=0; i<len ; i++) 
    hue2rgb(step*(len-1-i),cmap[i],cmap[i]+1,cmap[i]+2);
  
  return cmap;
}

void free_colormap(float **cmap)
{
  if (cmap != NULL) {
    free(*cmap);
    free(cmap);
  }
}


void set_light_on() {
  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
  glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_amb);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diff);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shine);
  glEnable(GL_LIGHT0);
  glColor3f(1.0, 1.0, 1.0);
  glFrontFace(GL_CCW);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void set_light_off() {
  glDisable(GL_LIGHTING);
  glColor3f(1.0, 1.0, 1.0);
  glFrontFace(GL_CCW);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

int do_normals(struct model* raw_model) {
  struct info_vertex *tmp;
  int i;

  printf("Computing normals...\n");
  if (raw_model->area == NULL)
    raw_model->area = (float*)malloc(raw_model->num_faces*sizeof(float));

  tmp = (struct info_vertex*)
    malloc(raw_model->num_vert*sizeof(struct info_vertex));
  
  raw_model->face_normals = compute_face_normals(raw_model, tmp);
  
  if (raw_model->face_normals != NULL){
    compute_vertex_normal(raw_model, tmp, raw_model->face_normals);
    for (i=0; i<raw_model->num_vert; i++) 
      free(tmp[i].list_face);
    free(tmp);
    printf("Face and vertex normals done !\n");
    return 0;
  } else {
    printf("Error - Unable to build face normals (Non-manifold model ?)\n");
    return 1;
  }
    
}

int do_spanning_tree(struct model *raw_model) {
  struct info_vertex* tmp;
  int i, ret=0;

  tmp = (struct info_vertex*)
    malloc(raw_model->num_vert*sizeof(struct info_vertex));
  for(i=0; i<raw_model->num_vert; i++) {
    tmp[i].list_face = (int*)malloc(sizeof(int));
    tmp[i].num_faces = 0;
  }
  printf("Building spanning tree\n");
  /* Compute spanning tree of the dual graph */
  raw_model->tree = bfs_build_spanning_tree(raw_model, tmp); 
  if (raw_model->tree == NULL) {
    printf("Unable to build spanning tree\n");
    ret = 1;
  }

  if (ret == 0) {
    printf("Spanning tree done\n");
  }
  for(i=0; i<raw_model->num_vert; i++) 
    free(tmp[i].list_face);
  free(tmp);
  return ret;
}

/* tree destructor */
void destroy_tree(struct face_tree *tree) {
  
  if (tree->left != NULL)
    destroy_tree(tree->left);
  if (tree->right != NULL)
    destroy_tree(tree->right);

  if (tree->left == NULL && tree->right == NULL) {
    if (tree->parent != NULL) {
      if (tree->node_type == 0)
	(tree->parent)->left = NULL;
      else
	(tree->parent)->right = NULL;
    }
    free(tree);

  }
}


int do_curvature(struct gl_render_context *gl_ctx) {
  int i;
  struct model* raw_model = gl_ctx->raw_model;
  face_t *cur_face;

  if (raw_model->area == NULL)
    raw_model->area = (float*)malloc(raw_model->num_faces*sizeof(float));

  for (i=0; i< raw_model->num_faces; i++) {
    cur_face = &(raw_model->faces[i]);
    raw_model->area[i] = tri_area_v(&(raw_model->vertices[cur_face->f0]), 
                                    &(raw_model->vertices[cur_face->f1]), 
                                    &(raw_model->vertices[cur_face->f2]));
  }
  gl_ctx->info = (struct info_vertex*)
    calloc(gl_ctx->raw_model->num_vert, sizeof(struct info_vertex));
  if (compute_curvature(gl_ctx->raw_model, gl_ctx->info)) {
    gl_ctx->disp_curv = 0;
    free(gl_ctx->info);
    gl_ctx->info = NULL;
    return 1;
  }
  
  /* if the computation was successful, find the max/min of the
   * Gauss. and mean curvatures.  */
  gl_ctx->max_kg = -FLT_MAX;
  gl_ctx->min_kg = FLT_MAX;
  
  gl_ctx->max_km = -FLT_MAX;
  gl_ctx->min_km = FLT_MAX;

  for (i=0; i<gl_ctx->raw_model->num_vert; i++) {
    /* Gauss curv. */
    if (gl_ctx->info[i].gauss_curv > gl_ctx->max_kg)
      gl_ctx->max_kg = gl_ctx->info[i].gauss_curv;

    if (gl_ctx->info[i].gauss_curv < gl_ctx->min_kg)
      gl_ctx->min_kg = gl_ctx->info[i].gauss_curv;

    /* Mean curv. */
    if (gl_ctx->info[i].mean_curv > gl_ctx->max_km)
      gl_ctx->max_km = gl_ctx->info[i].mean_curv;

    if (gl_ctx->info[i].mean_curv < gl_ctx->min_km)
      gl_ctx->min_km = gl_ctx->info[i].mean_curv;
  }

  return 0;
}
