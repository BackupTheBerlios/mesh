/* $Id: RawWidget.h,v 1.17 2001/09/25 13:17:43 dsanta Exp $ */
#ifndef RAWWIDGET_H
#define RAWWIDGET_H

#include <compute_error.h>
#include <qgl.h>

#define FOV 40.0 // Vertical field of view for the rendering

#define RW_LIGHT_TOGGLE 0
// If set, the widget has the right to switch from non-lighted mode to the
// lighted mode

#define RW_COLOR 1
// If set, the model has a color assigned per vertex

// These two flags are exclusive (typically the 1st model has RW_COLOR set
// while the 2nd one has RW_LIGHT_TOGGLE)
 
class RawWidget : public QGLWidget 
{ 

  Q_OBJECT 

public:  
  RawWidget(struct model_error *model, int renderType, QWidget *parent=0, 
	    const char *name=0); // Constructor

  
public slots: 
  void setLine(bool state);
  void setLight();
  void switchSync(bool state);
  void transfer(double dist,double *mvmat);

  
signals:
  void transfervalue(double,double*);
  void toggleSync();
  void toggleLine();
   
protected:
  void initializeGL();
  void resizeGL( int, int );   
  void mouseMoveEvent(QMouseEvent*);
  void mousePressEvent(QMouseEvent*);
  void keyPressEvent(QKeyEvent*);
  void paintGL();

private:  
// functions 
  void display(double distance);
  void rebuild_list();


// vars
  int renderFlag; // flag to indicate whether the widget can be set in
  // the lighted mode or not
  GLdouble dth, dph, dpsi;
  double **colormap;
  struct model_error *model;
  GLdouble distance, dstep;
  int oldx,oldy;
  GLdouble mvmatrix[16]; // Buffer for GL_MODELVIEW_MATRIX 
  GLuint model_list; // display list index for the model 
// state vars
  int left_button_state;
  int middle_button_state;
  int right_button_state;
  int move_state;
  int computed_normals; // flag if normals have been computed or loaded
  int not_orientable_warned;
  int two_sided_material;
};

#endif
