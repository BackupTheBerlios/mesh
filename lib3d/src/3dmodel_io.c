/* $Id: 3dmodel_io.c,v 1.14 2001/09/03 11:40:11 aspert Exp $ */
#include <3dmodel.h>
#include <normals.h>

int read_header(FILE *pf, int *nvert, int *nfaces, int *nnorms, int *nfnorms) {
  char buffer[300];
  char *tok1, *tok2, *tok3, *tok4, *delim;

    
  *nfaces = 0;
  *nvert = 0;
  *nnorms = 0;
  /* These are delimiters for the header */
  delim = (char*)malloc(2*sizeof(char));
  delim[0] = ' ';
  delim[1] = '\0';
  /* Scan 1st line */
  fgets(buffer, 200, pf);
  /* Extract tokens */
  tok1 = strtok(buffer, delim);
  tok2 = strtok(NULL, delim);
  tok3 = strtok(NULL, delim);
  tok4 = strtok(NULL, delim);
  free(delim);
  /* Check validity */
  if (tok4 != NULL) {
    *nfnorms = atoi(tok4);
    *nnorms = atoi(tok3);
    *nfaces = atoi(tok2);
    *nvert = atoi(tok1);
    if (*nfaces != *nfnorms) {
      fprintf(stderr, "Incorrect number of face normals\n");
      return 0;
    }
    if (*nvert != *nnorms) {
      fprintf(stderr, "Incorrect number of vertex normals\n");
      return 0;
    }
  } else if (tok3 != NULL) {
    *nnorms = atoi(tok3);
    *nfaces = atoi(tok2);
    *nvert = atoi(tok1);
    if (*nvert != *nnorms) {
      fprintf(stderr, "Incorrect number of normals\n");
      return 0;
    }
  } else if (tok1 != NULL && tok2 != NULL) {
    *nfaces = atoi(tok2);
    *nvert = atoi(tok1);
  } else {
    fprintf(stderr, "Invalid header\n");
    return 0;
  }
  return 1;
} 

model* alloc_read_model(FILE *pf, int nvert, int nfaces, int nnorms, int nfnorms) {
  model *raw_model;
  int i;
  double x,y,z;
  int v0, v1, v2;


  printf("num_faces = %d num_vert = %d\n", nfaces, nvert); 
  raw_model = (model*)malloc(sizeof(model));
  raw_model->num_faces = nfaces;
  raw_model->num_vert = nvert;
  raw_model->faces = (face*)malloc(nfaces*sizeof(face));
  raw_model->vertices = (vertex*)malloc(nvert*sizeof(vertex));

  raw_model->normals = NULL;
  raw_model->face_normals = NULL;
  raw_model->area = NULL;
  raw_model->tree = NULL;
#ifdef _METRO
  raw_model->error = NULL;
#endif

  raw_model->bBox[0].x = FLT_MAX;
  raw_model->bBox[0].y = FLT_MAX;
  raw_model->bBox[0].z = FLT_MAX;

  raw_model->bBox[1].x = -FLT_MAX;
  raw_model->bBox[1].y = -FLT_MAX;
  raw_model->bBox[1].z = -FLT_MAX;

  if (nfnorms > 0) {
    raw_model->face_normals = (vertex*)malloc(nfnorms*sizeof(vertex));
    raw_model->normals = (vertex*)malloc(nnorms*sizeof(vertex));
    raw_model->builtin_normals = 1;
  }
  else if (nnorms > 0) {
    raw_model->normals = (vertex*)malloc(nnorms*sizeof(vertex));
    raw_model->builtin_normals = 1;
  }
  else 
    raw_model->builtin_normals = 0;
  

  for (i=0; i<nvert; i++) {
    fscanf(pf,"%lf %lf %lf",&x, &y, &z);
    raw_model->vertices[i].x = 1.0*x;
    raw_model->vertices[i].y = 1.0*y;
    raw_model->vertices[i].z = 1.0*z;
     if (raw_model->vertices[i].x > raw_model->bBox[1].x) 
      raw_model->bBox[1].x = raw_model->vertices[i].x;
     if (raw_model->vertices[i].x < raw_model->bBox[0].x)
      raw_model->bBox[0].x = raw_model->vertices[i].x;

    if (raw_model->vertices[i].y > raw_model->bBox[1].y) 
      raw_model->bBox[1].y = raw_model->vertices[i].y;
    if (raw_model->vertices[i].y < raw_model->bBox[0].y)
      raw_model->bBox[0].y = raw_model->vertices[i].y;

    if (raw_model->vertices[i].z > raw_model->bBox[1].z) 
      raw_model->bBox[1].z = raw_model->vertices[i].z;
    if (raw_model->vertices[i].z < raw_model->bBox[0].z)
      raw_model->bBox[0].z = raw_model->vertices[i].z;
  }
  
  for (i=0; i<nfaces; i++) {
    fscanf(pf,"%d %d %d",&v0, &v1, &v2);
    raw_model->faces[i].f0 = v0;
    raw_model->faces[i].f1 = v1;
    raw_model->faces[i].f2 = v2;
  }
  
  if (nnorms > 0) {
    for (i=0; i<nnorms; i++) {
      fscanf(pf,"%lf %lf %lf",&x, &y, &z);
      raw_model->normals[i].x = x;
      raw_model->normals[i].y = y;
      raw_model->normals[i].z = z;
    }
  }
  if (nfnorms > 0) {
    for (i=0; i<nfnorms; i++) {
      fscanf(pf,"%lf %lf %lf",&x, &y, &z);
      raw_model->face_normals[i].x = x;
      raw_model->face_normals[i].y = y;
      raw_model->face_normals[i].z = z;
    }
  }
  

  return raw_model;
}


model* read_raw_model(char *filename) {
  model* raw_model;
  FILE *pf;
  int nfaces = 0; 
  int nvert = 0;
  int nnorms = 0;
  int nfnorms = 0;

  pf = fopen(filename, "r");
  if (pf==NULL) {
    perror("Error :");
    exit(-1);
  }
    
  if (!read_header(pf, &nvert, &nfaces, &nnorms, &nfnorms)) {
    fprintf(stderr, "Exitting\n");
    fclose(pf);
    exit(-1);
  } 

  raw_model = alloc_read_model(pf, nvert, nfaces, nnorms, nfnorms);

  fclose(pf);
  return raw_model;
  

}


model* read_raw_model_frame(char *filename,int frame) {
  model* raw_model;
  FILE *pf;
  char *fullname;
  char *tmp;
  int nfaces=0, nvert=0, len, nnorms=0, nfnorms=0;

  if (frame == -1) {
    len = strlen(filename)+20;
    fullname = (char*)malloc(len*sizeof(char));

    strcpy(fullname, filename);
    strcat(fullname,"_base.raw");
  }
  else {
    len = strlen(filename)+7+(int)log10(((frame>0)?frame:1))+1;
    tmp = (char*)malloc((8+(int)log10((frame>0)?frame:1))*sizeof(char));
    fullname = (char*)malloc(len*sizeof(char));
    
    strcpy(fullname,filename);
    sprintf(tmp,"_%d.raw",frame);
    strcat(fullname,tmp);

    free(tmp);   
  }

  pf = fopen(fullname,"r");
  free(fullname);
  
  if (pf==NULL) {
    perror("Error : ");
    exit(-1);
  }
  
  if (!read_header(pf, &nvert, &nfaces, &nnorms, &nfnorms)) {
    fclose(pf);
    exit(-1);
  }

  raw_model = alloc_read_model(pf, nvert, nfaces, nnorms, nfnorms);

  fclose(pf);
  return raw_model;
}



void write_raw_model(model *raw_model, char *filename) {
  FILE *pf;
  int i;
  char *rootname;
  char *finalname;
  char *tmp;
  int root_length;

  if (raw_model->builtin_normals == 0 && raw_model->normals != NULL) {

    tmp = strrchr(filename, '.'); /* find last occurence of '.' */

    if (tmp == NULL) /* filename does not have an extension */
	rootname = filename;
    else {
      if (*(tmp+1) == '/') /* filename does not have an extension */
	rootname = filename;
      else {
	root_length = tmp - filename; /* number of chars before extension */
	rootname = (char*)malloc((root_length+1)*sizeof(char));
	strncpy(rootname, filename, root_length*sizeof(char));
	rootname[root_length] = '\0'; /* strncpy does not add it */
      }
    }
    finalname = (char*)malloc((strlen(rootname)+7)*sizeof(char));
    sprintf(finalname, "%s_n.raw", rootname);
  } else
    finalname = filename;


  pf = fopen(finalname,"w");
  if (pf == NULL) {
    printf("Unable to open %s\n",filename);
    exit(0);
  }
  if (raw_model->normals == NULL) {
  fprintf(pf,"%d %d\n",raw_model->num_vert,raw_model->num_faces);
  for (i=0; i<raw_model->num_vert; i++)
    fprintf(pf, "%f %f %f\n",raw_model->vertices[i].x,
	    raw_model->vertices[i].y, raw_model->vertices[i].z);

  for (i=0; i<raw_model->num_faces; i++)
    fprintf(pf, "%d %d %d\n",raw_model->faces[i].f0,
	    raw_model->faces[i].f1,raw_model->faces[i].f2);
  } else {
    fprintf(pf,"%d %d %d %d\n",raw_model->num_vert,raw_model->num_faces, 
	    raw_model->num_vert, raw_model->num_faces);
    for (i=0; i<raw_model->num_vert; i++)
      fprintf(pf, "%f %f %f\n",raw_model->vertices[i].x,
	      raw_model->vertices[i].y, raw_model->vertices[i].z);
    
    for (i=0; i<raw_model->num_faces; i++)
      fprintf(pf, "%d %d %d\n",raw_model->faces[i].f0,
	      raw_model->faces[i].f1,raw_model->faces[i].f2);
    
    for (i=0; i<raw_model->num_vert; i++)
      fprintf(pf, "%f %f %f\n", raw_model->normals[i].x, 
	      raw_model->normals[i].y, raw_model->normals[i].z);

    for (i=0; i<raw_model->num_faces; i++)
      fprintf(pf, "%f %f %f\n", raw_model->face_normals[i].x,
	      raw_model->face_normals[i].y, raw_model->face_normals[i].z);
	 
  }
  fclose(pf);
}

void free_raw_model(model *raw_model) {
  free(raw_model->vertices);
  free(raw_model->faces);
  if (raw_model->normals != NULL)
    free(raw_model->normals);
  if (raw_model->face_normals != NULL)
    free(raw_model->face_normals);
  if (raw_model->area != NULL)
    free(raw_model->area);
  if (raw_model->tree != NULL)
    destroy_tree(*(raw_model->tree));

#ifdef _METRO
  if (raw_model->error != NULL)
    free(raw_model->error);
#endif

  free(raw_model);
}

void write_brep_file(model *raw_model, char *filename, int grid_size_x,
		     int grid_size_y, int  grid_size_z,
		     vertex bbox_min, vertex bbox_max) {

  FILE *pf;
  int i;
  
  pf = fopen(filename, "w");
  if (pf == NULL) {
    fprintf(stderr, "Unable to open output file %s\n", filename);
    exit(-1);
  }
  fprintf(pf, "%f %f %f %f %f %f\n",bbox_min.x, bbox_min.y, bbox_min.z, 
	  bbox_max.x, bbox_max.y, bbox_max.z);
  fprintf(pf, "%d %d %d\n", grid_size_x, grid_size_y, grid_size_z);
  fprintf(pf, "0\n0\n%d\n%d\n", raw_model->num_vert, raw_model->num_faces);
  for (i=0; i<raw_model->num_vert; i++)
    fprintf(pf, "%f %f %f\n", raw_model->vertices[i].x, 
	    raw_model->vertices[i].y, raw_model->vertices[i].z);
  for (i=0; i<raw_model->num_faces; i++)
    fprintf(pf, "%d %d %d\n", raw_model->faces[i].f0, raw_model->faces[i].f1,
	    raw_model->faces[i].f2);
  fclose(pf);
}
