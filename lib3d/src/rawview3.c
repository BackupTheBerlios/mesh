/* $Id: rawview3.c,v 1.6 2001/03/27 08:14:18 aspert Exp $ */
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <3dutils.h>
#include <image.h>

/* ****************** */
/* Useful Global vars */
/* ****************** */
GLfloat FOV = 40.0; /* vertical field of view */
GLdouble distance, dstep; /* distance and incremental distance step */
GLdouble mvmatrix[16]; /* Buffer for GL_MODELVIEW_MATRIX */
GLuint model_list = 0; /* display lists idx storage */
GLuint normal_list = 0;

int oldx, oldy;
int left_button_state;
int middle_button_state;
int right_button_state;
int tr_mode = 1; /* Default = draw triangles */
int light_mode = 0;
int draw_normals = 0;
vertex center;
int normals_done = 0;
model *raw_model;
char *in_filename;
int grab_number = 0;

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
  char filename[11];
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
  image_uchar_write(frame, pf);
  fclose(pf);
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
  GLdouble dth, dph, dpsi;

  dx = x - oldx;
  dy = y - oldy;

  if (left_button_state == 1) {
    dth = dx*0.5; /* Yes, 0.5 is arbitrary */
    dph = dy*0.5;
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
    dpsi = -dx*0.5;
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
void rebuild_list(model *raw_model) {
  int i;
  face *cur_face;

#ifdef FACE_NORM_DRAW_DEBUG
  vertex center; 
#endif

  if (glIsList(model_list) == GL_TRUE)
    glDeleteLists(model_list, 1);

  if (glIsList(normal_list) == GL_TRUE)
    glDeleteLists(normal_list, 1);
  model_list = glGenLists(1);

  if (draw_normals)
    normal_list = glGenLists(1);

#ifdef DEBUG
   printf("dn = %d lm = %d nf = %d\n", draw_normals, light_mode, 
	  raw_model->num_faces); 
#endif

  if (light_mode == 0) { /* Store a wireframe model */
    glNewList(model_list, GL_COMPILE);
    if (tr_mode == 1)
      glBegin(GL_TRIANGLES);
    else 
      glBegin(GL_POINTS);
    for (i=0; i<raw_model->num_faces; i++) {
      cur_face = &(raw_model->faces[i]);
      glVertex3d(raw_model->vertices[cur_face->f0].x,
		 raw_model->vertices[cur_face->f0].y,
		 raw_model->vertices[cur_face->f0].z); 

      glVertex3d(raw_model->vertices[cur_face->f1].x,
		 raw_model->vertices[cur_face->f1].y,
		 raw_model->vertices[cur_face->f1].z); 

      glVertex3d(raw_model->vertices[cur_face->f2].x,
		 raw_model->vertices[cur_face->f2].y,
		 raw_model->vertices[cur_face->f2].z); 
    }
    glEnd();
    glEndList();
    if (draw_normals) {
      glNewList(normal_list, GL_COMPILE);
      glColor3f(1.0, 0.0, 0.0);
      glBegin(GL_LINES);

#ifdef FACE_NORM_DRAW_DEBUG	
      for (i=0; i<raw_model->num_faces; i++) {
	cur_face = &(raw_model->faces[i]);
	center.x = (raw_model->vertices[cur_face->f0].x +
		    raw_model->vertices[cur_face->f1].x +
		    raw_model->vertices[cur_face->f2].x)/3.0;

	center.y = (raw_model->vertices[cur_face->f0].y +
		    raw_model->vertices[cur_face->f1].y +
		    raw_model->vertices[cur_face->f2].y)/3.0;

	center.z = (raw_model->vertices[cur_face->f0].z +
		    raw_model->vertices[cur_face->f1].z +
		    raw_model->vertices[cur_face->f2].z)/3.0;
	glVertex3d(center.x, center.y, center.z);
	glVertex3d(center.x + 0.1*raw_model->face_normals[i].x, 
		   center.y + 0.1*raw_model->face_normals[i].y, 
		   center.z + 0.1*raw_model->face_normals[i].z);
      }
#else
      for (i=0; i<raw_model->num_vert; i++) {
	glVertex3d(raw_model->vertices[i].x, 
		   raw_model->vertices[i].y,
		   raw_model->vertices[i].z);

	glVertex3d(raw_model->vertices[i].x + 0.3*raw_model->normals[i].x,
		   raw_model->vertices[i].y + 0.3*raw_model->normals[i].y,
		   raw_model->vertices[i].z + 0.3*raw_model->normals[i].z);
      }
#endif

      glEnd();
      glColor3f(1.0, 1.0, 1.0);
      glEndList();
    }
  } else {
    glNewList(model_list, GL_COMPILE);
    if (tr_mode == 1)
      glBegin(GL_TRIANGLES);
    else
      glBegin(GL_POINTS);
    for (i=0; i<raw_model->num_faces; i++) {
      cur_face = &(raw_model->faces[i]);
      glNormal3d(raw_model->normals[cur_face->f0].x,
		 raw_model->normals[cur_face->f0].y,
		 raw_model->normals[cur_face->f0].z);
      glVertex3d(raw_model->vertices[cur_face->f0].x,
		 raw_model->vertices[cur_face->f0].y,
		 raw_model->vertices[cur_face->f0].z);
      glNormal3d(raw_model->normals[cur_face->f1].x,
		 raw_model->normals[cur_face->f1].y,
		 raw_model->normals[cur_face->f1].z); 
      glVertex3d(raw_model->vertices[cur_face->f1].x,
		 raw_model->vertices[cur_face->f1].y,
		 raw_model->vertices[cur_face->f1].z);
      glNormal3d(raw_model->normals[cur_face->f2].x,
		 raw_model->normals[cur_face->f2].y,
		 raw_model->normals[cur_face->f2].z); 
      glVertex3d(raw_model->vertices[cur_face->f2].x,
		 raw_model->vertices[cur_face->f2].y,
		 raw_model->vertices[cur_face->f2].z); 
    }
    glEnd();
    glEndList();
    if (draw_normals) {
      glNewList(normal_list, GL_COMPILE);
      glColor3f(1.0, 0.0, 0.0);
      glBegin(GL_LINES);
      for (i=0; i<raw_model->num_vert; i++) {
	glVertex3d(raw_model->vertices[i].x, 
		   raw_model->vertices[i].y,
		   raw_model->vertices[i].z);
	glVertex3d(raw_model->vertices[i].x + raw_model->normals[i].x,
		   raw_model->vertices[i].y + raw_model->normals[i].y,
		   raw_model->vertices[i].z + raw_model->normals[i].z);
      }
      glEnd();
      glColor3f(1.0, 1.0, 1.0);
      glEndList();
    }
  }
}

/* ******************************************** */
/* Initial settings of the rendering parameters */
/* ******************************************** */
void gfx_init(model *raw_model) {

  glDepthFunc(GL_LESS);
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

/* **************************** */
/* Callback for the normal keys */
/* **************************** */
void norm_key_pressed(unsigned char key, int x, int y) {
  switch(key) {
  case 'q':
  case 'Q':
    free(raw_model->vertices);
    free(raw_model->faces);
    if (raw_model->normals != NULL)
      free(raw_model->normals);
    free(raw_model);
    exit(0);
    break;
  }
}

/* ********************************************************* */
/* Callback function for the special keys (arrows, function) */
/* ********************************************************* */
void sp_key_pressed(int key, int x, int y) {
  GLdouble tmp[16];
  GLfloat amb[] = {0.5, 0.5, 0.5, 1.0};
  GLfloat dif[] = {0.5, 0.5, 0.5, 1.0};
  GLfloat spec[] = {0.5, 0.5, 0.5, 0.5};
  GLfloat ldir[] = {0.0, 0.0, 0.0, 0.0};
  GLfloat mat_spec[] = {0.3, 0.7, 0.5, 0.5};
  GLfloat shine[] = {0.6};


/*   vertex *model_normals; */
  info_vertex *curv;
  int i;

  /* This one must be handled 'by hand' to please the MIPS compiler on SGI */
  GLfloat lpos[4];
  lpos[0] = 0.0;
  lpos[1] = 0.0;
  lpos[2] = -distance;
  lpos[3] = 0.0;

  switch(key) {
  case GLUT_KEY_F1:/* Print MODELVIEW matrix */
    glGetDoublev(GL_MODELVIEW_MATRIX, tmp);
    printf("\n");
    for (i=0; i<4; i++)
      printf("%f\t%f\t%f\t%f\n", tmp[4*i], tmp[4*i+1], tmp[4*i+2], 
	     tmp[4*i+3]); 
    break;
  case GLUT_KEY_F2: /* Toggle Light+filled mode */
    if (light_mode == 0) {
      light_mode = 1;
      printf("Lighted mode\n");
      if (normals_done != 1) {/* We have to build the normals */
	printf("Computing normals...");
	fflush(stdout);
	raw_model->area = (double*)malloc(raw_model->num_faces*sizeof(double));
	curv = (info_vertex*)malloc(raw_model->num_vert*sizeof(info_vertex));

	raw_model->face_normals = compute_face_normals(raw_model);

#ifdef FACE_NORM_DRAW_DEBUG	
	for (i=0; i<raw_model->num_faces; i++)
	  printf("%d: %f %f %f\n",i, model_normals[i].x, model_normals[i].y, 
		 model_normals[i].z);
#endif

	if (raw_model->face_normals != NULL){
	  compute_vertex_normal(raw_model, curv, raw_model->face_normals);
	  for (i=0; i<raw_model->num_vert; i++) 
	    free(curv[i].list_face);
	  free(curv);
	  normals_done = 1;
	  printf("done\n");
	
	  glEnable(GL_LIGHTING);
	  glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
	  glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
	  glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
	  glLightfv(GL_LIGHT0, GL_POSITION, lpos);
	  glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, ldir);
	  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
	  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shine);
	  glEnable(GL_LIGHT0);
	  glColor3f(1.0, 1.0, 1.0);
	  glFrontFace(GL_CCW);
	  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	  printf("Rebuild display list\n"); 
	  rebuild_list(raw_model);
	  glutPostRedisplay();
	} else {
	  printf("Unable to compute normals... non-manifold model\n");
	  light_mode = 0;
	}
      } else {
	glEnable(GL_LIGHTING);
	glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
	glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
	glLightfv(GL_LIGHT0, GL_POSITION, lpos);
	glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, ldir);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
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
    } else if (light_mode == 1) {
      light_mode = 0;
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
      for (i=0; i<raw_model->num_vert; i++) {
	raw_model->normals[i].x = -raw_model->normals[i].x;
	raw_model->normals[i].y = -raw_model->normals[i].y;
	raw_model->normals[i].z = -raw_model->normals[i].z;
      }

#ifdef FACE_NORM_DRAW_DEBUG
      for (i=0; i<raw_model->num_faces; i++) {
	raw_model->face_normals[i].x = -raw_model->face_normals[i].x;
	raw_model->face_normals[i].y = -raw_model->face_normals[i].y;
	raw_model->face_normals[i].z = -raw_model->face_normals[i].z;
      }
#endif

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
	raw_model->area = (double*)malloc(raw_model->num_faces*sizeof(double));
	curv = (info_vertex*)malloc(raw_model->num_vert*sizeof(info_vertex));
	raw_model->face_normals = compute_face_normals(raw_model);
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
  case GLUT_KEY_UP:
    glPushMatrix(); /* Save transform context */
    glLoadIdentity();
    glRotated(-1.0, 1.0, 0.0, 0.0);
    glMultMatrixd(mvmatrix); /* Add the sum of the previous ones */
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); /* Get the final matrix */
    glPopMatrix(); /* Reload previous transform context */
    glutPostRedisplay();
    break;
  case GLUT_KEY_DOWN:
    glPushMatrix(); 
    glLoadIdentity();
    glRotated(1.0, 1.0, 0.0, 0.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  case GLUT_KEY_LEFT:
    glPushMatrix();
    glLoadIdentity();
    glRotated(-1.0, 0.0, 1.0, 0.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  case GLUT_KEY_RIGHT:
    glPushMatrix();
    glLoadIdentity();
    glRotated(1.0, 0.0, 1.0, 0.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  case GLUT_KEY_PAGE_DOWN:
    glPushMatrix();
    glLoadIdentity();
    glRotated(-1.0, 0.0, 0.0, 1.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  case GLUT_KEY_END:
    glPushMatrix();
    glLoadIdentity();
    glRotated(1.0, 0.0, 0.0, 1.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glutPostRedisplay();
    break;
  }
}

/* ***************************************************************** */
/* Display function : clear buffers, build correct MODELVIEW matrix, */
/* call display list and swap the buffers                            */
/* ***************************************************************** */
void display() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  glTranslated(0.0, 0.0, -distance); /* Translate the object along z */
  glMultMatrixd(mvmatrix); /* Perform rotation */
  glCallList(model_list);
  if (draw_normals)
    glCallList(normal_list);
  glutSwapBuffers();
}

/* ************************************************************ */
/* Main function : read model, compute initial bounding box/vp, */
/* perform callback registration and go into the glutMainLoop   */
/* ************************************************************ */
int main(int argc, char **argv) {

  int i;


  if (argc != 2) {
    printf("rawview file.raw\n");
    exit(0);
  }
  

  in_filename = argv[1]; 
  raw_model = read_raw_model(in_filename);
  raw_model->face_normals = NULL;

  
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

  center.x = 0.5*(raw_model->bBox[1].x + raw_model->bBox[0].x);
  center.y = 0.5*(raw_model->bBox[1].y + raw_model->bBox[0].y);
  center.z = 0.5*(raw_model->bBox[1].z + raw_model->bBox[0].z);


  for (i=0; i<raw_model->num_vert; i++) {
    raw_model->vertices[i].x -= center.x;
    raw_model->vertices[i].y -= center.y;
    raw_model->vertices[i].z -= center.z;
  }
  


  distance = dist(raw_model->bBox[0], raw_model->bBox[1])/
    tan(FOV*M_PI_2/180.0);
  
  dstep = distance*0.01;


  /* Init the rendering window */
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(500, 500);
  glutCreateWindow("Raw Mesh Viewer v3.0");

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
