/* $Id: 3dmodel_io.c,v 1.9 2001/06/05 11:31:06 jacquet Exp $ */
#include <3dmodel.h>


int read_header(FILE *pf, int *nvert, int *nfaces, int *nnorms) {
  char buffer[300];
  char *tok1, *tok2, *tok3, *delim;

    
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
  free(delim);
  /* Check validity */
  if (tok3 != NULL) {
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

model* alloc_read_model(FILE *pf, int nvert, int nfaces, int nnorms) {
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

  raw_model->bBox[0].x = FLT_MAX;
  raw_model->bBox[0].y = FLT_MAX;
  raw_model->bBox[0].z = FLT_MAX;

  raw_model->bBox[1].x = -FLT_MAX;
  raw_model->bBox[1].y = -FLT_MAX;
  raw_model->bBox[1].z = -FLT_MAX;

  if (nnorms > 0) {
    raw_model->normals = (vertex*)malloc(nnorms*sizeof(vertex));
    raw_model->builtin_normals = 1;
  }
  else {
    raw_model->normals = NULL;
    raw_model->builtin_normals = 0;
  }

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
  raw_model->face_normals = NULL;
  raw_model->area = NULL;
  return raw_model;
}


model* read_raw_model(char *filename) {
  model* raw_model;
  FILE *pf;
  int nfaces=0; 
  int nvert=0;
  int nnorms=0;


  pf = fopen(filename, "r");
  if (pf==NULL) {
    perror("Error :");
    exit(-1);
  }
    
  if (read_header(pf, &nvert, &nfaces, &nnorms) == 0) {
    fclose(pf);
    exit(-1);
  } 

  raw_model = alloc_read_model(pf, nvert, nfaces, nnorms);

  fclose(pf);
  return raw_model;
  

}


model* read_raw_model_frame(char *filename,int frame) {
  model* raw_model;
  FILE *pf;
  char *fullname;
  char *tmp;
  int nfaces, nvert, len, nnorms;

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
  
  if (read_header(pf, &nvert, &nfaces, &nnorms) == 0) {
    fclose(pf);
    exit(-1);
  }

  raw_model = alloc_read_model(pf, nvert, nfaces, nnorms);

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
  } else if (raw_model->normals == NULL)
    finalname = filename;
  else /* This model already has normals, do nothing */
    return;

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
    fprintf(pf,"%d %d %d\n",raw_model->num_vert,raw_model->num_faces, 
	    raw_model->num_vert);
    for (i=0; i<raw_model->num_vert; i++)
      fprintf(pf, "%f %f %f\n",raw_model->vertices[i].x,
	      raw_model->vertices[i].y, raw_model->vertices[i].z);
    
    for (i=0; i<raw_model->num_faces; i++)
      fprintf(pf, "%d %d %d\n",raw_model->faces[i].f0,
	      raw_model->faces[i].f1,raw_model->faces[i].f2);
    
    for (i=0; i<raw_model->num_vert; i++)
      fprintf(pf, "%f %f %f\n", raw_model->normals[i].x, 
	      raw_model->normals[i].y, raw_model->normals[i].z);
	 
  }
  fclose(pf);
}

