/* $Id: mesh_run.c,v 1.3 2001/10/10 12:57:56 aspert Exp $ */

#include <time.h>
#include <string.h>
#include <xalloc.h>
#include <model_analysis.h>
#include <compute_error.h>
#include <compute_curvature.h>
#include <3dmodel_io.h>
#include <geomutils.h>

#include <mesh_run.h>

/* see mesh_run.h */
void mesh_run(const struct args *args, struct model_error *model1,
              struct model_error *model2, FILE *out)
{
  clock_t start_time;
  struct face_list *vfl;
  struct face_error *fe = NULL;
  struct face_error *fe_rev = NULL;
  struct dist_surf_surf_stats stats;
  struct dist_surf_surf_stats stats_rev;
  double bbox1_diag,bbox2_diag;
  struct model_info *m1info,*m2info;
  double abs_sampling_step;

  /* Read models from input files */
  memset(model1,0,sizeof(*model1));
  memset(model2,0,sizeof(*model2));
  m1info = (struct model_info*) xa_malloc(sizeof(*m1info));
  m2info = (struct model_info*) xa_malloc(sizeof(*m2info));
  model1->mesh = read_raw_model(args->m1_fname);
  model2->mesh = read_raw_model(args->m2_fname);

  /* Analyze models (we don't need normals for model 1, so we don't request
   * for it to be oriented). */
  start_time = clock();
  bbox1_diag = dist(model1->mesh->bBox[0], model1->mesh->bBox[1]);
  bbox2_diag = dist(model2->mesh->bBox[0], model2->mesh->bBox[1]);
  vfl = faces_of_vertex(model1->mesh);
  analyze_model(model1->mesh,vfl,m1info,0);
  model1->info = m1info;
  analyze_model(model2->mesh,NULL,m2info,1);
  model2->info = m2info;
  if(args->no_gui){
    free_face_lists(vfl,model1->mesh->num_vert);
    vfl = NULL;
  }
  /* Adjust sampling step size */
  abs_sampling_step = args->sampling_step*bbox2_diag;

  /* Print available model information */
  fprintf(out,"\n                      Model information\n\n");
  fprintf(out,"Number of vertices:     \t%11d\t%11d\n",
	  model1->mesh->num_vert,model2->mesh->num_vert);
  fprintf(out,"Number of triangles:    \t%11d\t%11d\n",
	  model1->mesh->num_faces,model2->mesh->num_faces);
  fprintf(out,"BoundingBox diagonal:   \t%11g\t%11g\n",
	  bbox1_diag,bbox2_diag);
  fprintf(out,"Number of disjoint parts:\t%11d\t%11d\n",
	  m1info->n_disjoint_parts,m2info->n_disjoint_parts);
  fprintf(out,"Manifold:               \t%11s\t%11s\n",
	  (m1info->manifold ? "yes" : "no"), 
	  (m2info->manifold ? "yes" : "no"));
  fprintf(out,"Originally oriented:    \t%11s\t%11s\n",
	  (m1info->orig_oriented ? "yes" : "no"),
	  (m2info->orig_oriented ? "yes" : "no"));
  fprintf(out,"Orientable:             \t%11s\t%11s\n",
	  (m1info->orientable ? "yes" : "no"),
	  (m2info->orientable ? "yes" : "no"));
  fprintf(out,"Closed:                 \t%11s\t%11s\n",
	  (m1info->closed ? "yes" : "no"),
	  (m2info->closed ? "yes" : "no"));
  fflush(stdout);

  /* Compute the distance from one model to the other */
  dist_surf_surf(model1->mesh,model2->mesh,abs_sampling_step,
                 &fe,&stats,!args->no_gui,args->quiet);

  /* Print results */
  fprintf(out,"Surface area:           \t%11g\t%11g\n",
         stats.m1_area,stats.m2_area);
  fprintf(out,"\n       Distance from model 1 to model 2\n\n");
  fprintf(out,"        \t   Absolute\t%% BBox diag\n");
  fprintf(out,"        \t           \t  (Model 2)\n");
  fprintf(out,"Min:    \t%11g\t%11g\n",
	  stats.min_dist,stats.min_dist/bbox2_diag*100);
  fprintf(out,"Max:    \t%11g\t%11g\n",
	  stats.max_dist,stats.max_dist/bbox2_diag*100);
  fprintf(out,"Mean:   \t%11g\t%11g\n",
	  stats.mean_dist,stats.mean_dist/bbox2_diag*100);
  fprintf(out,"RMS:    \t%11g\t%11g\n",
	  stats.rms_dist,stats.rms_dist/bbox2_diag*100);
  fprintf(out,"\n");
  fflush(stdout);
  
 

  if (args->do_symmetric) { /* Invert models and recompute distance */
    fprintf(out,"       Distance from model 2 to model 1\n\n");
    dist_surf_surf(model2->mesh,model1->mesh,abs_sampling_step,
                   &fe_rev,&stats_rev,0,args->quiet);
    free_face_error(fe_rev);
    fe_rev = NULL;
    fprintf(out,"        \t   Absolute\t%% BBox diag\n");
    fprintf(out,"        \t           \t  (Model 2)\n");
    fprintf(out,"Min:    \t%11g\t%11g\n",
           stats_rev.min_dist,stats_rev.min_dist/bbox2_diag*100);
    fprintf(out,"Max:    \t%11g\t%11g\n",
           stats_rev.max_dist,stats_rev.max_dist/bbox2_diag*100);
    fprintf(out,"Mean:   \t%11g\t%11g\n",
           stats_rev.mean_dist,stats_rev.mean_dist/bbox2_diag*100);
    fprintf(out,"RMS:    \t%11g\t%11g\n",
           stats_rev.rms_dist,stats_rev.rms_dist/bbox2_diag*100);
    fprintf(out,"\n");

    /* Print symmetric distance measures */
    fprintf(out,"       Symmetric distance between model 1 and model 2\n\n");
    fprintf(out,"        \t   Absolute\t%% BBox diag\n");
    fprintf(out,"        \t           \t  (Model 2)\n");
    fprintf(out,"Min:    \t%11g\t%11g\n",
           max(stats.min_dist,stats_rev.min_dist),
           max(stats.min_dist,stats_rev.min_dist)/bbox2_diag*100);
    fprintf(out,"Max:    \t%11g\t%11g\n",
           max(stats.max_dist,stats_rev.max_dist),
           max(stats.max_dist,stats_rev.max_dist)/bbox2_diag*100);
    fprintf(out,"Mean:   \t%11g\t%11g\n",
           max(stats.mean_dist,stats_rev.mean_dist),
           max(stats.mean_dist,stats_rev.mean_dist)/bbox2_diag*100);
    fprintf(out,"RMS:    \t%11g\t%11g\n",
           max(stats.rms_dist,stats_rev.rms_dist),
           max(stats.rms_dist,stats_rev.rms_dist)/bbox2_diag*100);
    fprintf(out,"\n");
  }


  fprintf(out,"               \t       Absolute\t   %% BBox diag model 2\n");
  fprintf(out,"Sampling step: \t%15g\t%22g\n",abs_sampling_step,
         abs_sampling_step/bbox2_diag*100);
  fprintf(out,"\n");
  if (!args->do_symmetric) {
    fprintf(out,"        \t    Total\tAvg. / triangle\t"
           "      Avg. / triangle\n"
           "        \t         \t     of model 1\t"
           "           of model 2\n");
    fprintf(out,"Samples:\t%9d\t%15.2f\t%21.2f\n",
           stats.m1_samples,((double)stats.m1_samples)/model1->mesh->num_faces,
           ((double)stats.m1_samples)/model2->mesh->num_faces);
  } else {
    fprintf(out,"                 \t    Total\tAvg. / triangle\t"
           "      Avg. / triangle\n"
           "                 \t         \t     of model 1\t"
           "           of model 2\n");
    fprintf(out,"Samples (1 to 2):\t%9d\t%15.2f\t%21.2f\n",
           stats.m1_samples,((double)stats.m1_samples)/model1->mesh->num_faces,
           ((double)stats.m1_samples)/model2->mesh->num_faces);
    fprintf(out,"Samples (2 to 1):\t%9d\t%15.2f\t%21.2f\n",
           stats_rev.m1_samples,
           ((double)stats_rev.m1_samples)/model1->mesh->num_faces,
           ((double)stats_rev.m1_samples)/model2->mesh->num_faces);
  }
  fprintf(out,"\n");
  if (!args->do_symmetric) {
    fprintf(out,"                       \t     X\t    Y\t   Z\t   Total\n");
    fprintf(out,"Partitioning grid size:\t%6d\t%5d\t%4d\t%8d\n",
           stats.grid_sz.x,stats.grid_sz.y,stats.grid_sz.z,
           stats.grid_sz.x*stats.grid_sz.y*stats.grid_sz.z);
  } else {
    fprintf(out,"                                \t     X\t    Y\t   Z\t   Total\n");
    fprintf(out,"Partitioning grid size (1 to 2):\t%6d\t%5d\t%4d\t%8d\n",
           stats.grid_sz.x,stats.grid_sz.y,stats.grid_sz.z,
           stats.grid_sz.x*stats.grid_sz.y*stats.grid_sz.z);
    fprintf(out,"Partitioning grid size (2 to 1):\t%6d\t%5d\t%4d\t%8d\n",
           stats_rev.grid_sz.x,stats_rev.grid_sz.y,stats_rev.grid_sz.z,
           stats_rev.grid_sz.x*stats_rev.grid_sz.y*stats_rev.grid_sz.z);
  }
  fprintf(out,"\n");
  
  if (args->do_curvature) {
    if (model1->mesh->num_vert == model2->mesh->num_vert)
      compute_curvature_error(model1, model2, out);
    else {
      fprintf(stderr, "Unable to compute curvature error for models having ");
      fprintf(stderr, "different topologies\n");
    }
  }
  fprintf(out,"Execution time (secs.):\t%.2f\n",
         (double)(clock()-start_time)/CLOCKS_PER_SEC);
  fflush(out);

  if(args->no_gui){
    free_face_error(fe);
    fe = NULL;
  } else {
    /* Get the per vertex error metric */
    calc_vertex_error(model1,fe,vfl);
    /* Free now useless data */
    free_face_error(fe);
    fe = NULL;
    free_face_lists(vfl,model1->mesh->num_vert);
    vfl = NULL;
  }
}
