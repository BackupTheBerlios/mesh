/* $Id: rawview.c,v 1.10 2002/06/04 09:27:10 aspert Exp $ */
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <3dutils.h>
#include <image.h>
#include <gl2ps.h>
#include <assert.h>

/* display normals with length=5% of the bounding box */
#define NORMALS_DISPLAY_FACTOR 5.0e-2 
/* step for rotation motion */
#define ANGLE_STEP 0.5
/* step for forward/backward motion */
#define TRANSL_STEP 1.0e-2
/* Field of view (in degrees) */
#define FOV 40.0

/* ****************** */
/* Useful Global vars */
/* ****************** */

GLfloat distance, dstep; /* distance and incremental distance step */
GLdouble mvmatrix[16]; /* Buffer for GL_MODELVIEW_MATRIX */


GLuint model_list = 0; /* display lists idx storage */
GLuint normal_list = 0;
GLuint tree_list = 0;
GLuint char_list = 0; /* those ones do not eat all the mem. with NV cards */

int oldx, oldy;
int left_button_state;
int middle_button_state;
int right_button_state;
int tr_mode = 1; /* Default = draw triangles */

int draw_normals = 0;
int draw_vtx_labels = 0;
int draw_spanning_tree = 0;
int wf_bc = 0; /* draw wireframe w. backface cull. */
int ps_rend = 0; /* 1 if we render to a PS */

vertex_t center;
int normals_done = 0;
struct model *raw_model;
char *in_filename;
int grab_number = 0;
int ps_number = 0;


struct model *r_model;


/* *********************************************************** */
/* This is a _basic_ frame grabber -> copy all the RGB buffers */
/* and put'em into grab$$$.ppm where $$$ is [000; 999]         */
/* This may kill the X server under undefined conditions       */
/* This function is called by pressing the 'F6' key            */
/* *********************************************************** */
void frame_grab() {
  int w,h,i;
  unsigned char *r_buffer, *g_buffer, *b_buffer;
  image_uchar *frame;
  char filename[12];
  int nbytes;
  FILE *pf;

  w = glutGet(GLUT_WINDOW_WIDTH);
  h = glutGet(GLUT_WINDOW_HEIGHT);
  r_buffer = (unsigned char*)malloc(w*h*sizeof(unsigned char));
  g_buffer = (unsigned char*)malloc(w*h*sizeof(unsigned char));
  b_buffer = (unsigned char*)malloc(w*h*sizeof(unsigned char));
  glReadBuffer(GL_FRONT);
  glReadPixels(0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, r_buffer);
  glReadPixels(0, 0, w, h, GL_GREEN, GL_UNSIGNED_BYTE, g_buffer);
  glReadPixels(0, 0, w, h, GL_BLUE, GL_UNSIGNED_BYTE, b_buffer);
  frame = image_uchar_alloc(w, h, 3, 255);
  nbytes = w*sizeof(unsigned char);
  for (i=0; i<h; i++) {
    memcpy(frame->data[0][i], &(r_buffer[w*(h-i)]), nbytes);
    memcpy(frame->data[1][i], &(g_buffer[w*(h-i)]), nbytes);
    memcpy(frame->data[2][i], &(b_buffer[w*(h-i)]), nbytes);
  }
  sprintf(filename,"grab%03d.ppm",grab_number);
  grab_number++;
  pf = fopen(filename,"w");
  if (pf == NULL) 
    fprintf(stderr,"Unable to open output file %s\n", filename);
  else {
    image_uchar_write(frame, pf);
    fclose(pf);
  }
  free(r_buffer);
  free(g_buffer);
  free(b_buffer);
  free_image_uchar(frame);
}


/* ************************************************************ */
/* Here is the callback function when mouse buttons are pressed */
/* or released. It does nothing else than store their state     */
/* ************************************************************ */
void mouse_button(int button, int state, int x, int y) {
switch(button) {
  case GLUT_LEFT_BUTTON:
    if (state==GLUT_DOWN) {
      oldx = x;
      oldy = y;
      left_button_state = 1;
    } else if (state == GLUT_UP)
      left_button_state = 0;
    break;
  case GLUT_MIDDLE_BUTTON:
    if (state==GLUT_DOWN) {
      oldx = x;
      oldy = y;
      middle_button_state = 1;
    } else if (state == GLUT_UP)
      middle_button_state = 0;
    break;
  case GLUT_RIGHT_BUTTON:
    if (state==GLUT_DOWN) {
      oldx = x;
      oldy = y;
      right_button_state = 1;
    } else if (state == GLUT_UP)
      right_button_state = 0;
    break;
  }
}


/* ********************************************************* */
/* Callback function when the mouse is dragged in the window */
/* Only does sthg when a button is pressed                   */
/* ********************************************************* */
void motion_mouse(int x, int y) {
  int dx, dy;
  GLfloat dth, dph, dpsi;

  dx = x - oldx;
  dy = y - oldy;

  if (left_button_state == 1) {
    dth = dx*ANGLE_STEP; 
    dph = dy*ANGLE_STEP;
    glPushMatrix(); /* Save transform context */
    glLoadIdentity();
    glRotated(dth, 0.0, 1.0, 0.0); /* Compute new rotation matrix */
    glRotated(dph, 1.0, 0.0, 0.0);
    glMultMatrixd(mvmatrix); /* Add the sum of the previous ones */
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); /* Get the final matrix */
    glPopMatrix(); /* Reload previous transform context */
    glutPostRedisplay();
  }
  else if (middle_button_state == 1) {
    distance += dy*dstep;
    glutPostRedisplay();
  }
  else if (right_button_state == 1) { 
    dpsi = -dx*ANGLE_STEP;
    glPushMatrix(); /* Save transform context */
    glLoadIdentity();
    glRotated(dpsi, 0.0, 0.0, 1.0); /* Modify roll angle */
    glMultMatrixd(mvmatrix);
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); /* Get the final matrix */
    glPopMatrix(); /* Reload previous transform context */
    glutPostRedisplay();
  }
  oldx = x;
  oldy = y;
}

/* ********************************************************** */
/* Reshape callbak function. Only sets correct values for the */
/* viewport and projection matrices.                          */
/* ********************************************************** */
void reshape(int width, int height) {
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(FOV, (GLdouble)width/(GLdouble)height, distance/10.0, 
		 10.0*distance);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  
}

/* ************************************************************* */
/* This functions rebuilds the display list of the current model */
/* It is called when the viewing setting (light...) are changed  */
/* ************************************************************* */
void rebuild_list(struct model *raw_model) {
  int i;
  GLboolean light_mode;
  float scale_fact;
  face_t *cur_face;
  vertex_t center1, center2;
  face_t *cur_face2;
  int j, face1=-1, face2=-1;

  /* delete all lists */
  if (glIsList(model_list) == GL_TRUE)
    glDeleteLists(model_list, 1);
  
  if (glIsList(normal_list) == GL_TRUE)
    glDeleteLists(normal_list, 1);
  
  if (glIsList(char_list) == GL_TRUE)
    glDeleteLists(char_list, 256);
  
  if (glIsList(tree_list) == GL_TRUE)
    glDeleteLists(tree_list, 1);
  
  /* create display lists indices */
  model_list = glGenLists(1);
  if (draw_spanning_tree)
    tree_list = glGenLists(1);
  
  if (draw_normals)
    normal_list = glGenLists(1);

  
  if (draw_vtx_labels) {
    char_list = glGenLists(256);
    if (char_list == 0)
      printf("Unable to create char_list\n");
    
    for (i=32; i<127; i++) { /* build display lists for labels */
      glNewList(char_list+i, GL_COMPILE);
      glutBitmapCharacter(GLUT_BITMAP_8_BY_13, i);
      glEndList();
    }
  }


  /* Get the state of the lighting */
  light_mode = glIsEnabled(GL_LIGHTING);
  if (light_mode && !glIsEnabled(GL_NORMAL_ARRAY)) {
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, (float*)(raw_model->normals));
  }
  if (!glIsEnabled(GL_VERTEX_ARRAY)) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, (float*)(raw_model->vertices));
  }
  

#ifdef DEBUG
  printf("dn = %d lm = %d nf = %d\n", draw_normals, light_mode, 
	 raw_model->num_faces); 
#endif
  

  if (draw_spanning_tree == 1) {

    glNewList(tree_list, GL_COMPILE);
    glColor3f(1.0, 1.0, 0.0);
    glBegin(GL_LINES);
    for (j=0; j<raw_model->num_faces; j++) {
      face1 = (raw_model->tree)[j]->face_idx;
      cur_face = &(raw_model->faces[face1]);
      add3_sc_v(0.333, &(raw_model->vertices[cur_face->f0]), 
		&(raw_model->vertices[cur_face->f1]), 
		&(raw_model->vertices[cur_face->f2]), &center1);
      if ((raw_model->tree)[j]->left != NULL) {
	face2 = ((raw_model->tree)[j]->left)->face_idx;
	cur_face2 = &(raw_model->faces[face2]);
	add3_sc_v(0.333, &(raw_model->vertices[cur_face2->f0]), 
		  &(raw_model->vertices[cur_face2->f1]), 
		  &(raw_model->vertices[cur_face2->f2]), &center2);
	glVertex3fv((float*)(&center1));
	glVertex3fv((float*)(&center2));

      }
      if ((raw_model->tree)[j]->right != NULL) {
	face2 = ((raw_model->tree)[j]->right)->face_idx;
	cur_face2 = &(raw_model->faces[face2]);
	add3_sc_v(0.333, &(raw_model->vertices[cur_face2->f0]), 
		  &(raw_model->vertices[cur_face2->f1]), 
		  &(raw_model->vertices[cur_face2->f2]), &center2);
	glVertex3fv((float*)(&center1));
	glVertex3fv((float*)(&center2));

      }
    }
    glEnd();
    glColor3f(1.0, 1.0, 1.0);
    glEndList();


  }



/* Model drawing (w. or wo. lighting) */
  glNewList(model_list, GL_COMPILE);
  if (tr_mode == 1)
    glBegin(GL_TRIANGLES);
  else 
    glBegin(GL_POINTS);
  for (i=0; i<raw_model->num_faces; i++) {
    cur_face = &(raw_model->faces[i]);
    glArrayElement(cur_face->f0);
    glArrayElement(cur_face->f1);
    glArrayElement(cur_face->f2);
  }
  glEnd();
  glEndList();



  if (draw_normals) {
    scale_fact = NORMALS_DISPLAY_FACTOR*dist_v(&(raw_model->bBox[0]), 
					       &(raw_model->bBox[1]));

    glNewList(normal_list, GL_COMPILE);
    glColor3f(1.0, 0.0, 0.0);
    glBegin(GL_LINES);
    for (i=0; i<raw_model->num_vert; i++) {
      glArrayElement(i);
      glVertex3f(raw_model->vertices[i].x + 
		 scale_fact*raw_model->normals[i].x,
		 raw_model->vertices[i].y + 
		 scale_fact*raw_model->normals[i].y,
		 raw_model->vertices[i].z + 
		 scale_fact*raw_model->normals[i].z);
    }
    glEnd();
    glColor3f(1.0, 1.0, 1.0);
    glEndList();
  }
  
}

/* ******************************************** */
/* Initial settings of the rendering parameters */
/* ******************************************** */
void gfx_init(struct model *raw_model) {
  const char *glverstr;

  glverstr = (const char*)glGetString(GL_VERSION);
  printf("GL_VERSION = %s\n", glverstr);




  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_SMOOTH); 

  glClearColor(0.0, 0.0, 0.0, 0.0);
  glColor3f(1.0, 1.0, 1.0); /* Settings for wireframe model */
  glFrontFace(GL_CCW);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  rebuild_list(raw_model);    

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(FOV, 1.0, distance/10.0, 10.0*distance);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); /* Initialize the temp matrix */
}


void display_vtx_labels() {
  int i, len;
  char str[42],fmt[24];

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glDisable(GL_LIGHTING);
  glColor3f(0.0, 1.0, 0.0);

  glListBase(char_list);
  strcpy(fmt, "%i");
  for (i=0; i<r_model->num_vert; i++) {
    glRasterPos3f(r_model->vertices[i].x,
		  r_model->vertices[i].y,
		  r_model->vertices[i].z);
    /* Add an offset to avoid drawing the label on the vertex*/
    glBitmap(0, 0, 0, 0, 7, 7, NULL); 
    len = sprintf(str, fmt, i);
    glCallLists(len, GL_UNSIGNED_BYTE, str);
  }
  
  glPopAttrib();
}


/* ***************************************************************** */
/* Display function : clear buffers, build correct MODELVIEW matrix, */
/* call display list and swap the buffers                            */
/* ***************************************************************** */
void display() {
  GLenum errorCode;
  GLboolean light_mode;
  GLfloat lpos[] = {-1.0, 1.0, 1.0, 0.0};
  int i;
  
  light_mode = glIsEnabled(GL_LIGHTING);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  glLightfv(GL_LIGHT0, GL_POSITION, lpos);
  glTranslated(0.0, 0.0, -distance); /* Translate the object along z */
  glMultMatrixd(mvmatrix); /* Perform rotation */
  if (!light_mode)
    for (i=0; i<=wf_bc; i++) {
      switch (i) {
      case 0:
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	if (!ps_rend)
	  glColor3f(1.0, 1.0, 1.0);
	else
	  glColor3f(0.0, 0.0, 0.0);
	break;
      case 1:
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (!ps_rend) {
	  glEnable(GL_POLYGON_OFFSET_FILL);
	  glPolygonOffset(1.0, 1.0);
	  glColor4f(0.0, 0.0, 0.0, 0.0);
	}
	else {
	  gl2psEnable(GL2PS_POLYGON_OFFSET_FILL);
	  glPolygonOffset(1.0, 1.0);
	  glColor4f(1.0, 1.0, 1.0, 0.0);
	}
	break;
      }

      glCallList(model_list);
  
      glDisable(GL_POLYGON_OFFSET_FILL);
      if (ps_rend)
	gl2psDisable(GL2PS_POLYGON_OFFSET_FILL);
    }
  else
    glCallList(model_list);

  if (draw_normals)
    glCallList(normal_list);


  if (draw_vtx_labels)
    display_vtx_labels();


  if(draw_spanning_tree)
    glCallList(tree_list);
  
  
 /* Check for errors (leave at the end) */
  while ((errorCode = glGetError()) != GL_NO_ERROR) {
    fprintf(stderr,"GL error: %s\n",(const char *)gluErrorString(errorCode));
  }
  glutSwapBuffers();

}

/* ***************************** */
/* Writes the frame to a PS file */
/* ***************************** */
void ps_grab() {
  int bufsize = 0, state = GL2PS_OVERFLOW;
  char filename[13];
  FILE *ps_file;

  sprintf(filename, "psgrab%03d.ps", ps_number);
  ps_file = fopen(filename, "w");
  if (ps_file == NULL)
    fprintf(stderr, "Unable to open PS outfile %s\n", filename);
  else {
    ps_rend = 1;
    glClearColor(1.0, 1.0, 1.0, 0.0);
    while (state == GL2PS_OVERFLOW) {
      bufsize += 1024*1024;
      gl2psBeginPage("PS Grab", "LaTeX", GL2PS_PS, GL2PS_SIMPLE_SORT, 
		     GL2PS_SIMPLE_LINE_OFFSET, 
		     GL_RGBA, 0, NULL, bufsize, ps_file, 
		     filename);
    
      display();
      state = gl2psEndPage();
    }
    ps_number++;
    printf("Buffer for PS grab was %d bytes\n", bufsize);
    ps_rend = 0;
    glClearColor(0.0, 0.0, 0.0, 0.0);
    fclose(ps_file);
  }
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



/* **************************** */
/* Callback for the normal keys */
/* **************************** */
void norm_key_pressed(unsigned char key, int x, int y) {
  switch(key) {
  case 'i':
  case 'I':
    fprintf(stderr, "\nModel Info :\n %d vertices and %d triangles\n\n", 
            raw_model->num_vert, raw_model->num_faces);
    break;
  case 'q':
  case 'Q':
    if (raw_model->tree != NULL)
      destroy_tree(*(raw_model->tree));
    __free_raw_model(raw_model);
    exit(0);
    break;
  }
}

/* ********************************************************* */
/* Callback function for the special keys (arrows, function) */
/* ********************************************************* */
void sp_key_pressed(int key, int x, int y) {

  /* Light specification */
  static const GLfloat amb[] = {0.1, 0.1, 0.1, 1.0};
  static const GLfloat dif[] = {1.0, 1.0, 1.0, 1.0};
  static const GLfloat spec[] = {1.0, 1.0, 1.0, 1.0};
  /* Global ambien light */
  static const GLfloat amb_light[] = {0.9, 0.9, 0.9, 1.0};
  /* Material specifications */
  static const GLfloat mat_spec[] = {0.33, 0.33, 0.52, 1.0};
  static const GLfloat mat_diff[] = {0.43, 0.47, 0.54, 1.0};
  static const GLfloat mat_amb[] = {0.11, 0.06, 0.11, 1.0};  
  static const GLfloat shine[] = {10.0};

  GLboolean light_mode;
  struct info_vertex *curv;
  struct face_tree* top;
  int i;



  
  light_mode = glIsEnabled(GL_LIGHTING);
  switch(key) {
  case GLUT_KEY_F1:/* Print Help */
    fprintf(stderr, "\n***********************\n");
    fprintf(stderr, "* Rawview v3.0 - Help *\n");
    fprintf(stderr, "***********************\n\n");
    fprintf(stderr, "F1 :\tDisplays this help\n");
    fprintf(stderr, "F2 :\tToggles lighted/wireframe mode\n");
    fprintf(stderr, "F3 :\tInvert normals (if any)\n");
    fprintf(stderr, "F4 :\tDraw vertex normals (if any)\n");
    fprintf(stderr, "F5 :\tSave model (incl. normals)\n");
    fprintf(stderr, "F6 :\tGrab the frame to a PPM file (grabxxx.ppm)\n");
    fprintf(stderr, "F7 :\tToggle triangle/point mode\n");
    fprintf(stderr, "F8 :\tDraw vertices' labels (be careful !)\n");
    fprintf(stderr, "F9 :\tDraw spanning tree (if any)\n");
    fprintf(stderr, "F10:\tRender in a PostScript file (uses 'gl2ps')\n");
    fprintf(stderr, "F11:\tToggle backface culling\n\n\n");
    fprintf(stderr, "Send bugs to Nicolas.Aspert@epfl.ch\n\t\t\tHave fun.\n");
    break;
  case GLUT_KEY_F2: /* Toggle Light+filled mode */
    if (light_mode == GL_FALSE) {
      printf("Lighted mode\n");
      if (normals_done != 1) {/* We have to build the normals */
	printf("Computing normals...\n");
	raw_model->area = (float*)malloc(raw_model->num_faces*sizeof(float));
	curv = (struct info_vertex*)
	  malloc(raw_model->num_vert*sizeof(struct info_vertex));

	raw_model->face_normals = compute_face_normals(raw_model, curv);

	if (raw_model->face_normals != NULL){
	  compute_vertex_normal(raw_model, curv, raw_model->face_normals);
	  for (i=0; i<raw_model->num_vert; i++) 
	    free(curv[i].list_face);
	  free(curv);
	  normals_done = 1;
	  printf("Normals done !\n");
	
	  glEnable(GL_LIGHTING);
 	  glLightfv(GL_LIGHT0, GL_AMBIENT, amb); 
	  glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
	  glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
	  glLightModelfv(GL_LIGHT_MODEL_AMBIENT,amb_light);
	  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
	  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_amb);
	  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diff);
	  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shine);
	  glEnable(GL_LIGHT0);
	  glColor3f(1.0, 1.0, 1.0);
	  glFrontFace(GL_CCW);
	  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	  printf("Rebuild display list\n"); 
	  rebuild_list(raw_model);
	  glutPostRedisplay();
	} else 
	  printf("Unable to compute normals... non-manifold model\n");
      } else {
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
	printf("Rebuild display list\n"); 
	rebuild_list(raw_model);
	glutPostRedisplay();
      }
      break;
    } else if (light_mode == GL_TRUE) {
      printf("Wireframe mode\n");
      glDisable(GL_LIGHTING);
      glColor3f(1.0, 1.0, 1.0);
      glFrontFace(GL_CCW);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      rebuild_list(raw_model);
      glutPostRedisplay();
      break;
    }
    printf("Something sucks...\n");
    break;
  case GLUT_KEY_F3: /* invert normals */
    if (light_mode || draw_normals) {
      printf("Inverting normals\n");
      for (i=0; i<raw_model->num_vert; i++) 
	neg_v(&(raw_model->normals[i]), &(raw_model->normals[i]));
      

      rebuild_list(raw_model);
      glutPostRedisplay();
    }
    break;
  case GLUT_KEY_F4: /* draw normals */
    if (draw_normals == 0) {
      draw_normals = 1;
      printf("Draw normals\n");
      if (normals_done != 1) {/* We have to build the normals */
	printf("Computing normals...");
	fflush(stdout);
	raw_model->area = (float*)malloc(raw_model->num_faces*sizeof(float));
	curv = (struct info_vertex*)
	  malloc(raw_model->num_vert*sizeof(struct info_vertex));
	raw_model->face_normals = compute_face_normals(raw_model, curv);
	if (raw_model->face_normals != NULL) {
	  compute_vertex_normal(raw_model, curv, raw_model->face_normals);
	  for (i=0; i<raw_model->num_vert; i++) 
	    free(curv[i].list_face);
	  free(curv);
	  normals_done = 1;
	  printf("done\n");	  
	} else {
	  printf("Unable to build normals (non-manifold model ?)\n");
	  draw_normals = 0;
	  break;
	}
      }
      printf("Rebuild display list\n"); 
      rebuild_list(raw_model);
      glutPostRedisplay();
      break;
    }

    else if (draw_normals == 1) {
      draw_normals = 0;
      printf("Rebuild display list\n"); 
      rebuild_list(raw_model);
      glutPostRedisplay();
      break;
    }
  case GLUT_KEY_F5: /* Save model... useful for normals */
    printf("Write model...\n");
    write_raw_model(raw_model, in_filename);
    break;
  case GLUT_KEY_F6: /* Frame grab */
    frame_grab();
    break;
  case GLUT_KEY_F7: /* switch from triangle mode to point mode */
    if(tr_mode == 1) {/*go to point mode*/
      tr_mode = 0;
      printf("Going to point mode\n");

    } else if(tr_mode == 0) {
      tr_mode = 1;
      printf("Going to triangle mode\n");
    }
    rebuild_list(raw_model);
    glutPostRedisplay();
    break;
  case GLUT_KEY_F8: /* draw labels for vertices */
    if (draw_vtx_labels == 1) {
      draw_vtx_labels = 0;
      printf("Stop drawing labels\n");
    } else if (draw_vtx_labels == 0) {
      draw_vtx_labels = 1;
      printf("Drawing labels\n");
    }
    rebuild_list(raw_model);
    glutPostRedisplay();
    break;

  case GLUT_KEY_F9: /* Draw the spanning tree */
    if(draw_spanning_tree == 1) {
      draw_spanning_tree = 0;
      printf("Stop drawing spanning tree\n");
    } else if (draw_spanning_tree == 0) {
      draw_spanning_tree = 1;
      printf("Drawing spanning tree\n");
      if (raw_model->tree == NULL) { /* We need to build this ...*/
	curv = (struct info_vertex*)
	  malloc(raw_model->num_vert*sizeof(struct info_vertex));
	for(i=0; i<raw_model->num_vert; i++) {
	  curv[i].list_face = (int*)malloc(sizeof(int));
	  curv[i].num_faces = 0;
	}
	printf("Building spanning tree\n");
	/* Compute spanning tree of the dual graph */
	raw_model->tree = bfs_build_spanning_tree(raw_model, curv); 
	if (raw_model->tree == NULL) {
	  printf("Uh oh... unable to build spanning tree\n");
          draw_spanning_tree =0;
          break;
        }
	top = raw_model->tree[0];
	while (top->parent != NULL)
	  top = top->parent;
	printf("Spanning tree done\n");
	for(i=0; i<raw_model->num_vert; i++) 
	  free(curv[i].list_face);
	free(curv);
      }
    }
    rebuild_list(raw_model);
    glutPostRedisplay();
    break;

    /* This NEEDS cleanup ! */
  case GLUT_KEY_F10: 
    printf("Rendering to a PostScript file...\n");
    ps_grab();
    printf("done\n");
    glutPostRedisplay();
    break;

  case GLUT_KEY_F11: /* backface culling when in wf mode */
    if (wf_bc) { /* goto classic wf mode */
      wf_bc = 0;
      glDisable(GL_LIGHTING);
      glColor3f(1.0, 1.0, 1.0);
      glFrontFace(GL_CCW);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      rebuild_list(raw_model);
    } else {
      wf_bc = 1;
      glDisable(GL_LIGHTING);
      glColor3f(1.0, 1.0, 1.0);
      glFrontFace(GL_CCW);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      rebuild_list(raw_model);
    }
    glutPostRedisplay();
    break;

  case GLUT_KEY_UP:
    glPushMatrix(); /* Save transform context */
    glLoadIdentity();
    glRotated(-5.0, 1.0, 0.0, 0.0);
    glMultMatrixd(mvmatrix); /* Add the sum of the previous ones */
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); /* Get the final matrix */
    glPopMatrix(); /* Reload previous transform context */
    glutPostRedisplay();
    break;
  case GLUT_KEY_DOWN:
    glPushMatrix(); 
    glLoadIdentity();
    glRotated(5.0, 1.0, 0.0, 0.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  case GLUT_KEY_LEFT:
    glPushMatrix();
    glLoadIdentity();
    glRotated(-5.0, 0.0, 1.0, 0.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  case GLUT_KEY_RIGHT:
    glPushMatrix();
    glLoadIdentity();
    glRotated(5.0, 0.0, 1.0, 0.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  case GLUT_KEY_PAGE_DOWN:
    glPushMatrix();
    glLoadIdentity();
    glRotated(-5.0, 0.0, 0.0, 1.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  case GLUT_KEY_END:
    glPushMatrix();
    glLoadIdentity();
    glRotated(5.0, 0.0, 0.0, 1.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  }
}






/* ************************************************************ */
/* Main function : read model, compute initial bounding box/vp, */
/* perform callback registration and go into the glutMainLoop   */
/* ************************************************************ */
int main(int argc, char **argv) {

  int i, rcode=0;
  char *title;
  const char s_title[]="Raw Mesh Viewer v3.0 - ";


  assert(sizeof(vertex_t) == 3*sizeof(float));
  if (argc != 2) {
    printf("Usage:%s file.[raw, wrl]\n", argv[0]);
    exit(-1);
  }

  in_filename = argv[1]; 


  rcode = read_fmodel(&raw_model, in_filename, MESH_FF_AUTO, 0);
  if (rcode < 0) {
    fprintf(stderr, "Unable to read model - error code %d\n", rcode);
    exit(-1);
  }  


  if (raw_model->builtin_normals == 1) {
    normals_done = 1;
    printf("The model has builtin normals\n");
  }

  
#ifdef DEBUG
  printf("bbox_min = %f %f %f\n", raw_model->bBox[0].x, 
	 raw_model->bBox[0].y, raw_model->bBox[0].z);
  printf("bbox_max = %f %f %f\n", raw_model->bBox[1].x, 
	 raw_model->bBox[1].y, raw_model->bBox[1].z);
#endif

  add_v(&(raw_model->bBox[0]), &(raw_model->bBox[1]), &center);
  prod_v(0.5, &center, &center);


  /* Center the model around (0, 0, 0) */
  for (i=0; i<raw_model->num_vert; i++) 
    substract_v(&(raw_model->vertices[i]), &center, &(raw_model->vertices[i]));
  
  
  


  distance = dist_v(&(raw_model->bBox[0]), &(raw_model->bBox[1]))/
    tan(FOV*M_PI_2/180.0);
  
  dstep = distance*TRANSL_STEP;


  r_model = raw_model;

  title = (char*)malloc((strlen(argv[1])+strlen(s_title)+1)*sizeof(char));
  strcpy(title, s_title);
  strcat(title, argv[1]);

  /* Init the rendering window */
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(500, 500);
  glutCreateWindow(title);
  free(title);

  /* Callback registration */
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutSpecialFunc(sp_key_pressed);
  glutKeyboardFunc(norm_key_pressed); 
  glutMouseFunc(mouse_button);
  glutMotionFunc(motion_mouse);

  /* 1st frame + build model */
  gfx_init(raw_model);

  /* Go for it */
  glutMainLoop();

  /* should never get here */
  return 0;
}
