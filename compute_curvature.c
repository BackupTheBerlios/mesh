/* $Id: compute_curvature.c,v 1.5 2001/10/25 12:30:45 aspert Exp $ */
#include <3dutils.h>
#include <compute_error.h>
#include <compute_curvature.h>

/* Returns the angle lying at vertex v0 in the triangle v0v1v2 */
double get_top_angle(const vertex_t *v0, const vertex_t *v1, 
		     const vertex_t *v2) {
  vertex_t u0, u1;
  double tmp;

  substract_v(v1, v0, &u0);
  substract_v(v2, v0, &u1);

  tmp = scalprod_v(&u0, &u1);
  tmp /= norm_v(&u0)*norm_v(&u1);
  return acos(tmp);

}

double get_top_angle2(const vertex_t *v0, const vertex_t *v1, 
		      const vertex_t *v2) {
  vertex_t u0, u1, v, h;
  double tmp;

  substract_v(v1, v0, &u0);
  substract_v(v2, v0, &u1);

  tmp = scalprod_v(&u0, &u1);
  prod_v(tmp/norm_v(&u0), &u0, &v);
  substract_v(&u1, &v, &h);
  if (fabs(tmp)< 1e-9)
    return M_PI_2;
  else if (tmp > 0.0)
    return atan2(norm_v(&h), norm_v(&v));
  else 
    return M_PI - atan2(norm_v(&h), norm_v(&v));


}

/* Test face f from raw_model to check if this is an obtuse triangle */
int obtuse_triangle(const vertex_t *v0, const vertex_t *v1, 
		    const vertex_t *v2) {
  double th, sum=0.0;

  th = get_top_angle(v0, v1, v2);
  sum += th;
  if (th > M_PI_2)
    return 1;
  th = get_top_angle(v2, v0, v1);
  sum += th;
  if (th > M_PI_2)
    return 1;
  if (M_PI-sum > M_PI_2)
    return 1;
  
  return 0;
}

void compute_mean_curvature_normal(const struct model *raw_model, 
				   struct info_vertex *info, 
				   int v0, const struct ring_info *rings, 
				   vertex_t *sum_vert, double *mixed_area, 
				   double *gauss_curv, double *mean_curv) {
  int v1, v1_idx, v2f, v2b, v2b_idx, v2, i;
  int n=rings[v0].size;
  vertex_t tmp;
  double alpha, beta, c, theta;
  face_t cur_face;

  *mixed_area = 0.0;
  sum_vert->x = 0.0;
  sum_vert->y = 0.0;
  sum_vert->z = 0.0;


  for (v1_idx=0; v1_idx<n; v1_idx++) {
    v1=rings[v0].ord_vert[v1_idx];
    v2f = rings[v0].ord_vert[(v1_idx + 1)%n];
    v2b_idx = (v1_idx > 0)?((v1_idx - 1)%n):(n - 1);
    v2b = rings[v0].ord_vert[v2b_idx];
    
    substract_v(&(raw_model->vertices[v1]), &(raw_model->vertices[v0]), &tmp);
    alpha = get_top_angle(&(raw_model->vertices[v2b]), 
			   &(raw_model->vertices[v1]), 
			   &(raw_model->vertices[v0]));
    beta = get_top_angle(&(raw_model->vertices[v2f]), 
			  &(raw_model->vertices[v1]), 
			  &(raw_model->vertices[v0]));
    c = (cos(alpha)/sin(alpha) + cos(beta)/sin(beta));

    add_prod_v(c, &tmp, sum_vert, sum_vert); 

  }
  
  *gauss_curv = 2.0*M_PI;

  for (i=0; i<info[v0].num_faces; i++) {
    cur_face = raw_model->faces[info[v0].list_face[i]];
    if (cur_face.f0 == v0) {
      v1 = cur_face.f1;
      v2 = cur_face.f2;
    } else if (cur_face.f1 == v0) {
      v1 = cur_face.f0;
      v2 = cur_face.f2;
    } else {
      v1 = cur_face.f0;
      v2 = cur_face.f1;
    }
    
    theta = get_top_angle(&(raw_model->vertices[v0]), 
			   &(raw_model->vertices[v1]),
			   &(raw_model->vertices[v2]));
    *gauss_curv -= theta; 

    if (!obtuse_triangle(&(raw_model->vertices[v0]), 
			 &(raw_model->vertices[v1]), 
			 &(raw_model->vertices[v2]))) {
      
      alpha = get_top_angle(&(raw_model->vertices[v1]), 
			     &(raw_model->vertices[v0]),
			     &(raw_model->vertices[v2]));
      beta =  get_top_angle(&(raw_model->vertices[v2]), 
			     &(raw_model->vertices[v1]),
			     &(raw_model->vertices[v0]));
      
      *mixed_area += 0.125*(dist2_v(&(raw_model->vertices[v0]), 
				    &(raw_model->vertices[v1]))*
			    cos(beta)/sin(beta) +
			    dist2_v(&(raw_model->vertices[v0]), 
				    &(raw_model->vertices[v2]))*
			    cos(alpha)/sin(alpha));

      
    }
    else {
      if (theta > M_PI_2) {
	*mixed_area += 0.5*tri_area_v(&(raw_model->vertices[cur_face.f0]), 
				      &(raw_model->vertices[cur_face.f1]), 
				      &(raw_model->vertices[cur_face.f2]));
      }
      else {
	*mixed_area += 0.25*tri_area_v(&(raw_model->vertices[cur_face.f0]), 
				       &(raw_model->vertices[cur_face.f1]), 
				       &(raw_model->vertices[cur_face.f2]));
      }
      
    }
  }
  
  prod_v(0.5/(*mixed_area), sum_vert, sum_vert);
  *gauss_curv /= *mixed_area;
  *mean_curv = 0.5*norm_v(sum_vert);
  
}


void compute_curvature(const struct model *raw_model, 
		       struct info_vertex *info, 
		       const struct ring_info *rings) {
  int i;
 

  for (i=0; i<raw_model->num_vert; i++) {
    if (rings[i].type != 0) {
      fprintf(stderr, "Unsupported vertex type %d found at %d\n", 
	      rings[i].type, i);
      return;
    }
    compute_mean_curvature_normal(raw_model, info, i, rings, 
				  &(info[i].mean_curv_normal), 
				  &(info[i].mixed_area), 
				  &(info[i].gauss_curv), 
				  &(info[i].mean_curv));


#ifdef __CURV_DEBUG
    printf("Vertex %d\n", i);
    printf("Mean curv. normal = %f %f %f\n", info[i].mean_curv_normal.x,
	   info[i].mean_curv_normal.y, info[i].mean_curv_normal.z);
    printf("Mixed area = %f\n", info[i].mixed_area);
    printf("Vertex normal = %f %f %f\n", raw_model->normals[i].x, 
	   raw_model->normals[i].y, raw_model->normals[i].z);
    printf("Vertex %d :Gauss_k=%f km=%f\n\n", i, info[i].gauss_curv,  
 	   info[i].mean_curv); 
#endif
  }
}

/* 
   This is the main method that computes the distortion of the 
   curvature of two models. They are assumed to have the SAME 
   number of vertices (test before calling the method)
   model1 should be the 'modified' model, whereas model2 stands
   for the original model 
*/
void compute_curvature_error(struct model_error *model1, 
			     struct model_error *model2, FILE *out) {

  int i;
  struct info_vertex *info1, *info2;
  struct ring_info *rings1, *rings2;


  printf("Computing face normals...\n");
  info1 = (struct info_vertex*)
    malloc(model1->mesh->num_vert*sizeof(struct info_vertex));
  model1->mesh->face_normals = compute_face_normals(model1->mesh, info1);


  info2 = (struct info_vertex*)
    malloc(model2->mesh->num_vert*sizeof(struct info_vertex));
  model2->mesh->face_normals = compute_face_normals(model2->mesh, info2);


  printf("Computing vertex normals...\n");
  model1->mesh->area = (double*)malloc(model1->mesh->num_faces*sizeof(double));
  model2->mesh->area = (double*)malloc(model2->mesh->num_faces*sizeof(double));
  compute_vertex_normal(model1->mesh, info1, 
			model1->mesh->face_normals); 
  compute_vertex_normal(model2->mesh, info2, 
			model2->mesh->face_normals); 

  printf("Generating 1-rings of all vertices...");fflush(stdout);
  rings1 = (struct ring_info*)
    malloc(model1->mesh->num_vert*sizeof(struct ring_info));
  rings2 = (struct ring_info*)
    malloc(model2->mesh->num_vert*sizeof(struct ring_info));
  for (i=0; i<model1->mesh->num_vert; i++) {
    build_star(model1->mesh, i, &(rings1[i]));
    build_star(model2->mesh, i, &(rings2[i]));
  }
  printf("done\n");

  printf("Computing curvature of model 1.... ");fflush(stdout);
  compute_curvature(model1->mesh, info1, rings1);
  printf("done\n");  
  printf("Computing curvature of model 2.... ");fflush(stdout);
  compute_curvature(model2->mesh, info2, rings2);
  printf("done\n\n");  
  

  /* The model 1 displays the curvature error when RawWidget is constructed */
  model1->km_error = (double*)malloc(model1->mesh->num_vert*sizeof(double));
  model1->kg_error = (double*)malloc(model1->mesh->num_vert*sizeof(double));

  model1->min_km_error = FLT_MAX;
  model1->max_km_error = -FLT_MAX;
  model1->mean_km_error = 0.0;

  model1->min_kg_error = FLT_MAX;
  model1->max_kg_error = -FLT_MAX;
  model1->mean_kg_error = 0.0;
  
  for (i=0; i<model1->mesh->num_vert; i++) {
    model1->km_error[i] = fabs(info1[i].mean_curv - info2[i].mean_curv);
    model1->kg_error[i] = fabs(info1[i].gauss_curv - info2[i].gauss_curv);
    
    if (model1->km_error[i] > model1->max_km_error)
      model1->max_km_error = model1->km_error[i];
    if (model1->kg_error[i] > model1->max_kg_error)
      model1->max_kg_error = model1->kg_error[i];

    if (model1->km_error[i] < model1->min_km_error)
      model1->min_km_error = model1->km_error[i];
    if (model1->kg_error[i] < model1->min_kg_error)
      model1->min_kg_error = model1->kg_error[i];
    

    model1->mean_km_error += info1[i].mixed_area*model1->km_error[i];
    model1->mean_kg_error += info1[i].mixed_area*model1->kg_error[i];
  }

  
  model1->mean_km_error /= model1->mesh->total_area;
  model1->mean_kg_error /= model1->mesh->total_area;

  /* Print the results */
  fprintf(out, "       Curvature difference between model 1 to model 2\n\n");
  fprintf(out, "        \t   Absolute\n");
  fprintf(out, "Min_KM :\t%11g\n", model1->min_km_error);
  fprintf(out, "Max_KM :\t%11g\n", model1->max_km_error);
  fprintf(out, "Min_KG :\t%11g\n", model1->min_kg_error);
  fprintf(out, "Max_KG :\t%11g\n", model1->max_kg_error);
  fprintf(out, "\n\n");
  
  fprintf(out, "       \t   Mean\n");
  fprintf(out, "KM    :\t%11g\n", model1->mean_km_error);
  fprintf(out, "KG    :\t%11g\n", model1->mean_kg_error);
  
}


