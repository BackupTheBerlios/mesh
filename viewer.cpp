/* $Id: viewer.cpp,v 1.30 2001/08/09 15:23:07 dsanta Exp $ */

#include <time.h>
#include <string.h>
#include <qapplication.h>
#include <qevent.h>
#include <qkeycode.h>

#include <compute_error.h>
#include <ScreenWidget.h>
#include <InitWidget.h>

#include <mutils.h>

/* To store the parsed arguments */
struct args {
  char *m1_fname; /* filename of model 1 */
  char *m2_fname; /* filename of model 2 */
  int  no_gui;    /* text only flag */
  int quiet;      /* do not display extra info flag*/
  int sampling_freq; /* sampling frequency */
};

/* Prints usage information to the out stream */
static void print_usage(FILE *out)
{
  fprintf(out,"Calculate and display the distance between two 3D models\n");
  fprintf(out,"\n");
  fprintf(out,"usage: viewer [[options] file1 file2]\n");
  fprintf(out,"\n");
  fprintf(out,"The program measures the distance from the 3D model in\n");
  fprintf(out,"file1 to the one in file2. The models must be given as\n");
  fprintf(out,"triangular meshes in RAW format, optionally with normals.\n");
  fprintf(out,"After the distance is calculated the result is displayed\n");
  fprintf(out,"as overall measures in text form and as a detailed distance\n");
  fprintf(out,"map in graphical form.\n");
  fprintf(out,"If no options nor filenames are given a dialog is shown\n");
  fprintf(out,"to select the input file names as well as the parameters.\n");
  fprintf(out,"\n");
  fprintf(out,"options:");
  fprintf(out,"\n");
  fprintf(out,"  -h\tDisplays this help message and exits.\n");
  fprintf(out,"\n");
  fprintf(out,"  -q\tQuiet, do not print progress meter.\n");
  fprintf(out,"\n");
  fprintf(out,"  -t\tDisplay only textual results, do not displaythe GUI.\n");
  fprintf(out,"\n");
  fprintf(out,"  -f n\tSet the sampling frequency to n. The triangles of\n");
  fprintf(out,"      \tthe first model are sampled in order to calculate\n");
  fprintf(out,"      \tan approximation of the distance. Each triangle\n");
  fprintf(out,"      \tis sampled n times along each side direction,\n");
  fprintf(out,"      \tresulting in n*(n+1)/2 samples per triangle. The\n");
  fprintf(out,"      \thigher the sampling frequency, the more accurate the\n");
  fprintf(out,"      \tapproximation, but the longer the execution time.\n");
  fprintf(out,"      \tIt is 10 by default.\n");
}

/* Initializes *pargs to default values and parses the command line arguments
 * in argc,argv. */
static void parse_args(int argc, char **argv, struct args *pargs)
{
  char *endptr;
  int i;

  memset(pargs,0,sizeof(*pargs));
  pargs->sampling_freq = 10;
  i = 1;
  while (i < argc) {
    if (argv[i][0] == '-') { /* Option */
      if (strcmp(argv[i],"-h") == 0) { /* help */
        print_usage(stdout);
        exit(0);
      } else if (strcmp(argv[i],"-t") == 0) { /* text only */
        pargs->no_gui = 1;
      } else if (strcmp(argv[i],"-q") == 0) { /* quiet */
        pargs->quiet = 1;
      } else if (strcmp(argv[i],"-f") == 0) { /* sampling freq */
        pargs->sampling_freq = strtol(argv[++i],&endptr,10);
        if (argv[i][0] == '\0' || *endptr != '\0' || pargs->sampling_freq < 0) {
          fprintf(stderr,"ERROR: invalid number for -f option\n");
          exit(1);
        }
      } else { /* unrecognized option */
        fprintf(stderr,
                "ERROR: unknown option in command line, use -h for help\n");
        exit(1);
      }
    } else { /* file name */
      if (pargs->m1_fname == NULL) {
        pargs->m1_fname = argv[i];
      } else if (pargs->m2_fname == NULL) {
        pargs->m2_fname = argv[i];
      } else {
        fprintf(stderr,
                "ERROR: too many arguments in command line, use -h for help\n");
        exit(1);
      }
    }
    i++; /* next argument */
  }
}

/*****************************************************************************/
/*             fonction principale                                           */
/*****************************************************************************/


int main( int argc, char **argv )
{
  clock_t start_time;
  model *raw_model1, *raw_model2;
  double dmoy,dmoymax=0,dmoymin=DBL_MAX;
  struct face_list *vfl;
  int i,j;
  double surfacemoy=0;
  QString m1,n1,o1;
  struct face_error *fe = NULL;
  struct dist_surf_surf_stats stats;
  double bbox1_diag,bbox2_diag;
  double *tmp_error;
  struct args pargs;

  /* affichage de la fenetre graphique */
  QApplication::setColorSpec( QApplication::CustomColor );
  QApplication a( argc, argv ); 

  /* Parse arguments */
  parse_args(argc,argv,&pargs);
  /* Display starting dialog if insufficient arguments */
  if (argc > 1) {
    if (pargs.m1_fname == NULL || pargs.m2_fname == NULL) {
      fprintf(stderr,"ERROR: missing file name(s) in command line\n");
      exit(1);
    }
  } else {
    InitWidget *b;
    b = new InitWidget() ;
    a.setMainWidget( b );
    b->show(); 
    a.exec();
    
    pargs.m1_fname = b->mesh1;
    pargs.m2_fname = b->mesh2;
    pargs.sampling_freq = (int)ceil(1.0/atof(b->step));
  }
  
  /* Read models from input files */
  raw_model1=read_raw_model(pargs.m1_fname);
  raw_model2=read_raw_model(pargs.m2_fname);

  /* Compute the distance from one model to the other */
  start_time = clock();
  dist_surf_surf(raw_model1,raw_model2,pargs.sampling_freq,&fe,&stats,
                 pargs.quiet);

  /* Print results */
  bbox1_diag = dist(raw_model1->bBox[0],raw_model1->bBox[1]);
  bbox2_diag = dist(raw_model2->bBox[0],raw_model2->bBox[1]);
  printf("\n              Model information\n\n");
  printf("                \t    Model 1\t    Model 2\n");
  printf("Number of vertices:\t%11d\t%11d\n",
         raw_model1->num_vert,raw_model2->num_vert);
  printf("Number of triangles:\t%11d\t%11d\n",
         raw_model1->num_faces,raw_model2->num_faces);
  printf("BoundingBox diagonal:\t%11g\t%11g\n",
         bbox1_diag,bbox2_diag);
  printf("Surface area:   \t%11g\t%11g\n",
         stats.m1_area,stats.m2_area);
  printf("\n       Distance from model 1 to model 2\n\n");
  printf("        \t   Absolute\t%% BBox diag\n");
  printf("        \t           \t  (Model 2)\n");
  printf("Min:    \t%11g\t%11g\n",
         stats.min_dist,stats.min_dist/bbox2_diag);
  printf("Max:    \t%11g\t%11g\n",
         stats.max_dist,stats.max_dist/bbox2_diag);
  printf("Mean:   \t%11g\t%11g\n",
         stats.mean_dist,stats.mean_dist/bbox2_diag);
  printf("RMS:    \t%11g\t%11g\n",
         stats.rms_dist,stats.rms_dist/bbox2_diag);
  printf("\n");
  printf("Calculated error in %g seconds\n",
         (double)(clock()-start_time)/CLOCKS_PER_SEC);
  printf("Used %d samples per triangle of model 1 (%d total)\n",
         (pargs.sampling_freq)*(pargs.sampling_freq+1)/2,
         (pargs.sampling_freq)*(pargs.sampling_freq+1)/2*raw_model1->num_faces);
  printf("Size of partitioning grid (X,Y,Z): %d %d %d (%d total)\n",
         stats.grid_sz.x,stats.grid_sz.y,stats.grid_sz.z,
         stats.grid_sz.x*stats.grid_sz.y*stats.grid_sz.z);
  fflush(stdout);

  if(!pargs.no_gui){
    /* Get the faces incident on each vertex to assign error values to each
     * vertex for display. */
    vfl = faces_of_vertex(raw_model1);

   /* on assigne une couleur a chaque vertex qui est proportionnelle */
   /* a la moyenne de l'erreur sur les faces incidentes */
   raw_model1->error=(int *)malloc(raw_model1->num_vert*sizeof(int));
   raw_model2->error=(int *)calloc(raw_model2->num_vert,sizeof(int));



   tmp_error = (double*)malloc(raw_model1->num_vert*sizeof(double));

   for(i=0; i<raw_model1->num_vert; i++){
     dmoy = 0;
     surfacemoy = 0;
     for(j=0; j<vfl[i].n_faces; j++) {
       dmoy += fe[vfl[i].face[j]].mean_error*fe[vfl[i].face[j]].face_area;
       surfacemoy += fe[vfl[i].face[j]].face_area;
     }
     dmoy /= surfacemoy;
     tmp_error[i] = dmoy;

     if(dmoy>dmoymax)
       dmoymax = dmoy;

     if(dmoy<dmoymin)
       dmoymin = dmoy;
   }
   
   for(i=0;i<raw_model1->num_vert;i++){
     raw_model1->error[i] = 
       (int)floor(7*(tmp_error[i] - dmoymin)/(dmoymax - dmoymin));
     if(raw_model1->error[i]<0)
       raw_model1->error[i]=0;
   }
   
   free(tmp_error);

   
   ScreenWidget c(raw_model1, raw_model2, dmoymin, dmoymax);
   a.setMainWidget( &c );
   c.show(); 
   a.exec();

 }
 
}
