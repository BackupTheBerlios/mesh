/* $Id: RawWidget.cpp,v 1.24 2001/09/25 13:17:43 dsanta Exp $ */

#include <RawWidget.h>
#include <qmessagebox.h>
#include <ColorMap.h>

// 
// This is a derived class from QGLWidget used to render models
// 

RawWidget::RawWidget(struct model_error *model, int renderType, 
		     QWidget *parent, const char *name)
  :QGLWidget(parent, name) { 
  
  int i;
  vertex center;

  // Fixed size widget
  setMinimumSize(512, 512);
  setMaximumSize(512, 512);

  // 0 is not a valid display list index
  model_list = 0;
  
  // Build the colormap used to display the mean error onto the surface of
  // the model
  colormap = HSVtoRGB();

  // Get the structure containing the model
  this->model = model;

  // Get the flags
  renderFlag = renderType;

  // Initialize the state
  move_state=0;
  not_orientable_warned = 0;
  two_sided_material = 1;

  // Compute the center of the bounding box of the model
  center.x = 0.5*(model->mesh->bBox[1].x + model->mesh->bBox[0].x);
  center.y = 0.5*(model->mesh->bBox[1].y + model->mesh->bBox[0].y);
  center.z = 0.5*(model->mesh->bBox[1].z + model->mesh->bBox[0].z);

  // Center the model around (0, 0, 0)
  for (i=0; i<model->mesh->num_vert; i++) {
    model->mesh->vertices[i].x -= center.x;
    model->mesh->vertices[i].y -= center.y;
    model->mesh->vertices[i].z -= center.z;
  }
  
  // This should be enough to see the whole model when starting
  distance = dist(model->mesh->bBox[0], model->mesh->bBox[1])/
    tan(FOV*M_PI_2/180.0);

  // This is the increment used when moving closer/farther from the object
  dstep = distance*0.01;




}


void RawWidget::transfer(double dist,double *mvmat) {

  distance = dist;
  // Copy the 4x4 transformation matrix
  memcpy(mvmatrix, mvmat, 16*sizeof(double)); 
  // update display
  makeCurrent();
  glDraw();
}

void RawWidget::switchSync(bool state) {
  if (state) {
    move_state = 1;
    emit(transfervalue(distance, mvmatrix));
  } else
    move_state = 0;
}


void RawWidget::setLine(bool state) {
  // state=TRUE -> switch to line
  // state=FALSE -> switch to fill
  GLint line_state[2]; // front and back values

  // Forces the widget to be the current context. Undefined otherwise
  // and this causes a _silly_ behaviour !
  makeCurrent();

  glGetIntegerv(GL_POLYGON_MODE,line_state);
  if (line_state[0]==GL_FILL && line_state[1]==GL_FILL && state==TRUE) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDraw();
  } else if (line_state[0]==GL_LINE && line_state[1]==GL_LINE && 
	     state==FALSE) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDraw(); 
  } else {
    printf("Invalid state value found for GL_POLYGON_MODE: %d %d\n",
           line_state[0],line_state[1]);
  }
}

void RawWidget::setLight() {
  GLboolean light_state;

  makeCurrent();
  // Get state from renderer
  if (renderFlag == RW_LIGHT_TOGGLE) {
    light_state = glIsEnabled(GL_LIGHTING);

    if (light_state==GL_FALSE){ // We are now switching to lighted mode
      if (model->mesh->normals !=NULL){// Are these ones computed ?
	glEnable(GL_LIGHTING);
      } else {// Normals should have been computed
        fprintf(stderr,"ERROR: normals where not computed!\n");
      }
    }
    else if (light_state==GL_TRUE){// We are now switching to wireframe mode
      glDisable(GL_LIGHTING);
    }
    glDraw();
  }
}


// display callback
void RawWidget::paintGL() {
  display(distance);
}

// resize callback
void RawWidget::resizeGL(int width ,int height) {
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(FOV, (GLdouble)width/(GLdouble)height, distance/10.0, 
		 10.0*distance);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  
}

// Initializations for the renderer
void RawWidget::initializeGL() { 
  static const GLfloat amb[] = {0.1f, 0.1f, 0.1f, 1.0f};
  static const GLfloat dif[] = {0.3f, 0.3f, 0.3f, 1.0f};
  static const GLfloat spec[] = {0.3f, 0.3f, 0.3f, 0.3f};
  static const GLfloat amb_light[] = {0.8f, 0.8f, 0.8f, 1.0f};

  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_SMOOTH);


  glClearColor(0.0, 0.0, 0.0, 0.0);

  glLightfv(GL_LIGHT0, GL_AMBIENT, amb); 
  glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
  glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT,amb_light);
  if (!model->info->closed) glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
  glEnable(GL_LIGHT0);
  glFrontFace(GL_CCW);

  rebuild_list();
  glEndList();
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(FOV, 1.0, distance/10.0, 10.0*distance);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); /* Initialize the temp matrix */
  if (renderFlag == RW_LIGHT_TOGGLE) {
    if (model->mesh->normals !=NULL){// Are these ones computed ?
      if (!model->info->orientable && !not_orientable_warned) {
        not_orientable_warned = 1;
        QMessageBox::warning(this,"Not orientable model",
                             "Model is not orientable.\n"
                             "Surface shading is probably incorrect.");
      }
      glEnable(GL_LIGHTING);
    } else {// Normals should have been computed
      fprintf(stderr,"ERROR: normals where not computed!\n");
    }
  }
}  


// 'display' function called by the paintGL call back
// clears the buffers, computes correct transformation matrix
// and calls the model's display list
void RawWidget::display(double distance) {
  GLfloat lpos[] = {-1.0, 1.0, 1.0, 0.0} ;

  makeCurrent();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  /* Set the light position relative to eye point */
  glLightfv(GL_LIGHT0, GL_POSITION, lpos);  
  glTranslated(0.0, 0.0, -distance); /* Translate the object along z axis */
  glMultMatrixd(mvmatrix); /* Perform rotation */
  glCallList(model_list);
}


// This function generates the model's display list, depending on the
// viewing parameters (light...)
void RawWidget::rebuild_list() {
  // Surface material characteristics for lighted mode
  static const float front_amb_mat[4] = {0.5f, 0.5f, 0.5f, 1.0f};
  static const float front_diff_mat[4] = {0.7f, 0.7f, 0.7f, 1.0f};
  static const float front_spec_mat[4] = {0.3f, 0.3f, 0.3f, 1.0f};
  static const float front_mat_shin = 30.0f;
  static const float back_amb_mat[4] = {0.3f, 0.3f, 0.3f, 1.0f};
  static const float back_diff_mat[4] = {0.5f, 0.5f, 0.5f, 1.0f};
  static const float back_spec_mat[4] = {0.2f, 0.2f, 0.2f, 1.0f};
  static const float back_mat_shin = 10.0f;
  // Color for non-lighted mode
  static const float lighted_color[3] = {1.0f, 1.0f, 1.0f};
  // Local vars
  int i,cidx;
  float drange;
  face *cur_face;

  if (glIsList(model_list)==GL_TRUE) 
    glDeleteLists(model_list, 1);

  model_list=glGenLists(1);
  
  if (renderFlag == RW_COLOR) {
    drange = model->max_verror-model->min_verror;
    if (drange < FLT_MIN*100) drange = 1;
    glNewList(model_list, GL_COMPILE);
    glBegin(GL_TRIANGLES);  
    for (i=0; i<model->mesh->num_faces; i++) {
      cur_face = &(model->mesh->faces[i]);
      cidx = (int) floor(7*(model->verror[cur_face->f0]-
                            model->min_verror)/drange);
      glColor3dv(colormap[cidx]);
      glVertex3d(model->mesh->vertices[cur_face->f0].x,
		 model->mesh->vertices[cur_face->f0].y,
		 model->mesh->vertices[cur_face->f0].z); 
      
      cidx = (int) floor(7*(model->verror[cur_face->f1]-
                            model->min_verror)/drange);
      glColor3dv(colormap[cidx]);
      glVertex3d(model->mesh->vertices[cur_face->f1].x,
		 model->mesh->vertices[cur_face->f1].y,
		 model->mesh->vertices[cur_face->f1].z); 
      
      cidx = (int) floor(7*(model->verror[cur_face->f2]-
                            model->min_verror)/drange);
      glColor3dv(colormap[cidx]);
      glVertex3d(model->mesh->vertices[cur_face->f2].x,
		 model->mesh->vertices[cur_face->f2].y,
		 model->mesh->vertices[cur_face->f2].z);       
    }
    glEnd();
    glEndList();
  }   
  else if (renderFlag == RW_LIGHT_TOGGLE) {
    glNewList(model_list, GL_COMPILE);
    glColor3fv(lighted_color);
    if (two_sided_material) {
      glMaterialfv(GL_FRONT,GL_AMBIENT,front_amb_mat);
      glMaterialfv(GL_FRONT,GL_DIFFUSE,front_diff_mat);
      glMaterialfv(GL_FRONT,GL_SPECULAR,front_spec_mat);
      glMaterialf(GL_FRONT,GL_SHININESS,front_mat_shin);
      glMaterialfv(GL_BACK,GL_AMBIENT,back_amb_mat);
      glMaterialfv(GL_BACK,GL_DIFFUSE,back_diff_mat);
      glMaterialfv(GL_BACK,GL_SPECULAR,back_spec_mat);
      glMaterialf(GL_BACK,GL_SHININESS,back_mat_shin);
    } else {
      glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,front_amb_mat);
      glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,front_diff_mat);
      glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,front_spec_mat);
      glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,front_mat_shin);
    }
    if (model->mesh->normals != NULL) {
      glBegin(GL_TRIANGLES);  
      for (i=0; i<model->mesh->num_faces; i++) {
	cur_face = &(model->mesh->faces[i]);
	
	
	glNormal3d(model->mesh->normals[cur_face->f0].x,
		   model->mesh->normals[cur_face->f0].y,
		   model->mesh->normals[cur_face->f0].z);
	glVertex3d(model->mesh->vertices[cur_face->f0].x,
		   model->mesh->vertices[cur_face->f0].y,
		   model->mesh->vertices[cur_face->f0].z); 
	
	glNormal3d(model->mesh->normals[cur_face->f1].x,
		   model->mesh->normals[cur_face->f1].y,
		   model->mesh->normals[cur_face->f1].z);  
	glVertex3d(model->mesh->vertices[cur_face->f1].x,
		   model->mesh->vertices[cur_face->f1].y,
		   model->mesh->vertices[cur_face->f1].z); 
	
	glNormal3d(model->mesh->normals[cur_face->f2].x,
		   model->mesh->normals[cur_face->f2].y,
		   model->mesh->normals[cur_face->f2].z); 
	glVertex3d(model->mesh->vertices[cur_face->f2].x,
		   model->mesh->vertices[cur_face->f2].y,
		   model->mesh->vertices[cur_face->f2].z);       
      }
      glEnd();
    } else {
      glBegin(GL_TRIANGLES);  
      for (i=0; i<model->mesh->num_faces; i++) {
	cur_face = &(model->mesh->faces[i]);
	
	glVertex3d(model->mesh->vertices[cur_face->f0].x,
		   model->mesh->vertices[cur_face->f0].y,
		   model->mesh->vertices[cur_face->f0].z); 
	
	glVertex3d(model->mesh->vertices[cur_face->f1].x,
		   model->mesh->vertices[cur_face->f1].y,
		   model->mesh->vertices[cur_face->f1].z); 
	
	glVertex3d(model->mesh->vertices[cur_face->f2].x,
		   model->mesh->vertices[cur_face->f2].y,
		   model->mesh->vertices[cur_face->f2].z);       
      }
      glEnd();
    }
    glEndList();
  }

}

/* ************************************************************ */
/* Here is the callback function when mouse buttons are pressed */
/* or released. It does nothing else than store their state     */
/* ************************************************************ */
void RawWidget::mousePressEvent(QMouseEvent *event)
{
  if(event->button() & LeftButton){
    left_button_state=1;
    oldx=event->x();
    oldy=event->y();
  }
else 
  left_button_state=0;
  if(event->button() & RightButton){
    right_button_state=1;
    oldx=event->x();
    oldy=event->y();
  }
else 
  right_button_state=0;
  if(event->button() & MidButton){
    middle_button_state=1;
    oldx=event->x();
    oldy=event->y();
  }
else 
  middle_button_state=0;
}

/* ********************************************************* */
/* Callback function when the mouse is dragged in the window */
/* Only does sthg when a button is pressed                   */
/* ********************************************************* */
void RawWidget::mouseMoveEvent(QMouseEvent *event) {
  int dx,dy;

  dx= event->x() - oldx;
  dy= event->y() - oldy;

  if(left_button_state==1){  
    dth = dx*0.5; 
    dph = dy*0.5;
    glPushMatrix(); 
    glLoadIdentity();
    glRotated(dth, 0.0, 1.0, 0.0);
    glRotated(dph, 1.0, 0.0, 0.0);
    glMultMatrixd(mvmatrix); 
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); 
    glPopMatrix(); 
    glDraw();
  }
  else if (middle_button_state == 1) {
    distance += dy*dstep;
    glDraw();
  }
  else if (right_button_state == 1) { 
    dpsi = -dx*0.5;
    glPushMatrix(); /* Save transform context */
    glLoadIdentity();
    glRotated(dpsi, 0.0, 0.0, 1.0); /* Modify roll angle */
    glMultMatrixd(mvmatrix);
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix); /* Get the final matrix */
    glPopMatrix(); /* Reload previous transform context */
    glDraw(); 
  }

  if(move_state==1)
    emit(transfervalue(distance, mvmatrix));
  oldx = event->x();
  oldy = event->y();

}

void RawWidget::keyPressEvent(QKeyEvent *k) {
  GLboolean light_state;
  int i;
  
  switch(k->key()) {
  case Key_F1:
    emit toggleLine();
    break;
  case Key_F2:
    setLight();
    break;
  case Key_F3:
    // if we are going to sync make sure other widgets get out transformation
    // matrix first
    if (move_state==0) emit transfervalue(distance, mvmatrix);
    // now send the signal that we need to toggle synchronization
    emit toggleSync();
    break;
  case Key_F4:
    makeCurrent();
    if (renderFlag == RW_LIGHT_TOGGLE) {
      light_state = glIsEnabled(GL_LIGHTING);
      if (light_state == GL_TRUE) { // Invert normals
	for (i=0; i<model->mesh->num_vert; i++) {
	  model->mesh->normals[i].x = -model->mesh->normals[i].x;
	  model->mesh->normals[i].y = -model->mesh->normals[i].y;
	  model->mesh->normals[i].z = -model->mesh->normals[i].z;
	}
	rebuild_list();
	glDraw();
      }
    }
    break;
  case Key_F5:
    if (renderFlag == RW_LIGHT_TOGGLE) {
      makeCurrent();
      two_sided_material = !two_sided_material;
      rebuild_list();
      glDraw();
    }
    break;
  default:
    break;
  }
}


