/* $Id: mesh.cpp,v 1.18 2002/02/20 23:43:50 dsanta Exp $ */

#include <time.h>
#include <string.h>
#include <qapplication.h>
#include <qprogressdialog.h>
#include <ScreenWidget.h>
#include <InitWidget.h>

#include <mesh_run.h>
#include <3dmodel_io.h>

/* Prints usage information to the out stream */
static void print_usage(FILE *out)
{
  fprintf(out,"MESH: Measuring Distance between Surfaces with the "
          "Hausdorff distance\n");
  fprintf(out,"\n");
  fprintf(out,"usage: mesh [[options] file1 file2]\n");
  fprintf(out,"\n");
  fprintf(out,"The program measures the distance from the 3D model in\n");
  fprintf(out,"file1 to the one in file2. The models must be given as\n");
  fprintf(out,"triangular meshes in RAW or VRML2 formats, optionally with\n");
  fprintf(out,"normals. The VRML parser reads IndexedFaceSets nodes only,\n");
  fprintf(out,"ignoring all transformations, and does not support USE tags\n");
  fprintf(out,"(DEF tags are ignored). The file type is autodetected.\n");
  fprintf(out,"After the distance is calculated the result is displayed\n");
  fprintf(out,"as overall measures in text form and as a detailed distance\n");
  fprintf(out,"map in graphical form.\n");
  fprintf(out,"If no options nor filenames are given a dialog is shown\n");
  fprintf(out,"to select the input file names as well as the parameters.\n");
  fprintf(out,"\n");
  fprintf(out,"Display:\n");
  fprintf(out,"After calculating the distance, the error distribution on\n");
  fprintf(out,"model 1 is be displayed in graphical form. There are three\n");
  fprintf(out,"modes to display the error distribution: vertex error, face\n");
  fprintf(out,"mean error and sample error. In vertex error, each vertex is\n");
  fprintf(out,"assigned a color based on the calculated error at that\n");
  fprintf(out,"vertex (if not calculated at that vertex, dark gray is used)\n");
  fprintf(out,"and the color is interpolated between vertices. While fast,\n");
  fprintf(out,"this method ignores any errors not located at the vertices\n");
  fprintf(out,"and thus can provide misleading results. In mean face error,\n");
  fprintf(out,"each triangle is assigned a color based on the mean error\n");
  fprintf(out,"for that face (if that face has no samples a dark gray is\n");
  fprintf(out,"used). In sample error mode a color is assigned to each\n");
  fprintf(out,"sample point based on the error at that point, and applied\n");
  fprintf(out,"to the model using texture mapping (if a face has no sample\n");
  fprintf(out,"points a dark gray is assigned to it). This mode is the\n");
  fprintf(out,"slowest but it accurately represents the error distribution.\n");
  fprintf(out,"A colorbar showing the correspondance between error values\n");
  fprintf(out,"and color is displayed. The colorbar also shows the\n");
  fprintf(out,"approximate distribution, on the surface of model 1, of the\n");
  fprintf(out,"error values using a histogram (approximate because the\n");
  fprintf(out,"distribution of the sample points on the surface is not\n");
  fprintf(out,"truly uniform).\n");
  fprintf(out,"Note that the -f flag forces at least one sample per face.\n");
  fprintf(out,"\n");
  fprintf(out,"options:");
  fprintf(out,"\n");
  fprintf(out,"  -h\tDisplays this help message and exits.\n");
  fprintf(out,"\n");
  fprintf(out,"  -s\tCalculate a symmetric distance measure. It calculates\n");
  fprintf(out,"    \tthe distance in the two directions and uses the max\n");
  fprintf(out,"    \tas the symmetric distance (Hausdorff distance).\n");
  fprintf(out,"\n");
  fprintf(out,"  -q\tQuiet, do not print progress meter.\n");
  fprintf(out,"\n");
  fprintf(out,"  -t\tDisplay only textual results, do not display the GUI.\n");
  fprintf(out,"\n");
  fprintf(out,"  -l s\tSet the sampling step to s, which is a percentage of\n");
  fprintf(out,"      \tthe bounding box diagonal of the second model. The\n");
  fprintf(out,"      \ttriangles of the first model are sampled, in order\n");
  fprintf(out,"      \tto calculate an approximation of the distance, so\n");
  fprintf(out,"      \tthat the sampling density (number of samples per\n");
  fprintf(out,"      \tunit surface) is 1/(s^2) (i.e. one sample per square\n");
  fprintf(out,"      \tof side length s). A probabilistic model is used so\n");
  fprintf(out,"      \tthat the resulting number is as close as possible to\n");
  fprintf(out,"      \tthe target. The default is 0.5\n\n");
  fprintf(out,"  -mf f\tEnsure that each triangle has a sampling frequency\n");
  fprintf(out,"       \tof at least f. Normally the sampling frequency for\n");
  fprintf(out,"       \ta triangle is determined by the sampling step and\n");
  fprintf(out,"       \ttriangle size. For some combinations this can lead\n");
  fprintf(out,"       \tto some triangles having no or few samples. With a\n");
  fprintf(out,"       \tnon-zero f parameter this can be avoided, although\n");
  fprintf(out,"       \tit disturbs the uniform distribution of samples.\n");
  fprintf(out,"       \tWith f set to 1, all triangles get at least one\n");
  fprintf(out,"       \tsample. With f set to 2, all triangles get at least\n");
  fprintf(out,"       \tthree samples, and thus all vertices get a sample.\n");
  fprintf(out,"       \tHigher values of f are less useful. By default it\n");
  fprintf(out,"       \tis zero in non-GUI mode and two in GUI mode.\n");
  fprintf(out,"  -wlog\tDisplay textual results in a window instead of on\n");
  fprintf(out,"       \tstandard output. Not compatible with the -t option.\n");
  fprintf(out,"\n");
}

/* Initializes *pargs to default values and parses the command line arguments
 * in argc,argv. */
static void parse_args(int argc, char **argv, struct args *pargs)
{
  char *endptr;
  int i;

  memset(pargs,0,sizeof(*pargs));
  pargs->sampling_step = 0.5;
  pargs->min_sample_freq = -1;
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
      } else if (strcmp(argv[i],"-s") == 0) { /* symmetric distance */
        pargs->do_symmetric = 1;
      } else if (strcmp(argv[i],"-l") == 0) { /* sampling step */
        if (argc <= i+1) {
          fprintf(stderr,"ERROR: missing argument for -l option\n");
          exit(1);
        }
        pargs->sampling_step = strtod(argv[++i],&endptr);
        if (argv[i][0] == '\0' || *endptr != '\0' ||
            pargs->sampling_step <= 0) {
          fprintf(stderr,"ERROR: invalid number for -l option\n");
          exit(1);
        }
      } else if (strcmp(argv[i], "-mf") == 0) { /* sample all triangles */
        if (argc <= i+1) {
          fprintf(stderr,"ERROR: missing argument for -mf option\n");
          exit(1);
        }
        pargs->min_sample_freq = strtol(argv[++i],&endptr,10);
        if (argv[i][0] == '\0' || *endptr != '\0' ||
            pargs->min_sample_freq < 0) {
          fprintf(stderr,"ERROR: invalid number for -mf option\n");
          exit(1);
        }
      } else if (strcmp(argv[i], "-wlog") == 0) { /* log into window */
	pargs->do_wlog = 1;
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
  if (pargs->no_gui && pargs->do_wlog) {
    fprintf(stderr, "ERROR: incompatible options -t and -wlog\n");
    exit(1);
  }
  if (pargs->min_sample_freq < 0) {
    pargs->min_sample_freq = (pargs->no_gui) ? 0 : 2;
  }
  pargs->sampling_step /= 100; /* convert percent to fraction */
}

/*****************************************************************************/
/*             fonction principale                                           */
/*****************************************************************************/


int main( int argc, char **argv )
{
  int i;
  QString m1,n1,o1;
  struct args pargs;
  QApplication *a;
  InitWidget *b;
  ScreenWidget *c; 
  TextWidget *textOut;
  QProgressDialog *qProg;
  struct model_error model1,model2;
  int rcode;
  struct outbuf *log;
  struct prog_reporter pr;

  /* Initialize application */
  a = NULL;
  b = NULL;
  c = NULL;
  qProg = NULL;
  memset(&model1,0,sizeof(model1));
  memset(&model2,0,sizeof(model2));
  memset(&pr,0,sizeof(pr));
  log = NULL;
  i = 0;
  while (i<argc) {
    if (strcmp(argv[i],"-t") == 0) /* text version requested */
      break; 
    if (strcmp(argv[i],"-h") == 0) /* just asked for command line help */
      break; 
    i++;
  }
  if (i == argc) { /* no text version requested, initialize QT */
    a = new QApplication( argc, argv );
    if (a != NULL) a->connect( a, SIGNAL(lastWindowClosed()), 
			       a, SLOT(quit()) );
  } else {
    a = NULL; /* No QT app needed */
  }

  /* Parse arguments */
  parse_args(argc,argv,&pargs);
  /* Display starting dialog if insufficient arguments */
  if (pargs.m1_fname != NULL || pargs.m2_fname != NULL) {
    if (pargs.m1_fname == NULL || pargs.m2_fname == NULL) {
      fprintf(stderr,"ERROR: missing file name(s) in command line\n");
      exit(1);
    }
    if (!pargs.do_wlog) {
      log = outbuf_new(stdio_puts,stdout);
    }
    else {
      textOut = new TextWidget();
      log = outbuf_new(TextWidget_puts,textOut);
      textOut->show();
    }
    if (pargs.no_gui) {
      pr.prog = stdio_prog;
      pr.cb_out = stdout;
    } else {
      qProg = new QProgressDialog("Calculating distance",0,100);
      qProg->setMinimumDuration(1500);
      pr.prog = QT_prog;
      pr.cb_out = qProg;
    }
    mesh_run(&pargs,&model1,&model2, log, &pr);
  } else {
    b = new InitWidget(pargs, &model1, &model2);
    b->show(); 
  }
  if (a != NULL) {
    if (pargs.m1_fname != NULL || pargs.m2_fname != NULL) {
      c = new ScreenWidget(&model1, &model2);
      a->setMainWidget(c);
      c->show(); 
    }
    rcode = a->exec();
  } else {
    rcode = 0;
  }
  /* Free widgets */
  outbuf_delete(log);
  delete qProg;
  delete b;
  delete c;
  delete a; // QApplication must be last QT thing to delete
  /* Free model data */
  if (model1.mesh != NULL) free_raw_model(model1.mesh);
  free(model1.verror);
  free(model1.info);
  if (model2.mesh != NULL) free_raw_model(model2.mesh);
  free(model2.verror);
  free(model2.info);
  /* Return exit code */
  return rcode;
}
