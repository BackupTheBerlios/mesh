/* $Id: cone.c,v 1.1 2001/09/12 13:42:45 aspert Exp $ */
#include <3dutils.h>

int main(int argc, char **argv) {
  int valence, i, fcount=0;
  double h, step, th=0.0;
  char *out_fname;
  model *cone;

  if (argc != 4) {
    fprintf(stderr, "Usage: cone height valence outfile\n");
    exit(-1);
  }
  
  h = atof(argv[1]);
  valence = atoi(argv[2]);
  out_fname = argv[3];
  
  cone = (model*)malloc(sizeof(model));
  cone->num_vert = valence + 2;
  cone->vertices = (vertex*)malloc(cone->num_vert*sizeof(vertex));
  cone->num_faces = 2*valence;
  cone->faces = (face*)malloc(cone->num_faces*sizeof(face));

  cone->vertices[0].x = 0.0;
  cone->vertices[0].y = 0.0;
  cone->vertices[0].z = 0.0;

  cone->vertices[1].x = 0.0;
  cone->vertices[1].y = 0.0;
  cone->vertices[1].z = h;

  step = 2*M_PI/(double)valence;

  for (i=2; i<cone->num_vert; i++, th+=step) {
    cone->vertices[i].x = cos(th);
    cone->vertices[i].y = sin(th);
    cone->vertices[i].z = 0.0;
    cone->faces[fcount].f0 = i;
    cone->faces[fcount].f1 = ((i+1)==cone->num_vert)?2:i+1;
    cone->faces[fcount++].f2 = 1;
    cone->faces[fcount].f0 = i;
    cone->faces[fcount].f1 = ((i+1)==cone->num_vert)?2:i+1;
    cone->faces[fcount++].f2 = 0;
  }

  write_raw_model(cone, out_fname);

  free_raw_model(cone);
  return 0;
}
