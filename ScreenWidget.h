/* $Id: ScreenWidget.h,v 1.20 2002/02/26 10:27:48 dsanta Exp $ */
#ifndef SCREENWIDGET_H
#define SCREENWIDGET_H

/* QT includes */
#include <qwidget.h>
#include <qgroupbox.h>
#include <compute_error.h>


class ScreenWidget : public QWidget {
  Q_OBJECT
public:
  ScreenWidget(struct model_error *model1, struct model_error *model2,
               int do_texture=0, QWidget *parent=0, const char *name=0);


protected slots:
  void quit();
  void infoLeftModel();
  void infoRightModel(); 
  void changeGroupBoxTitle(int n);
  void disableSlider(int errMode);
  void aboutKeys();
  void aboutBugs();
  void aboutMesh();

private:
  QGroupBox *qgbSlider;
// local copies of the parameters passed to the constructor
  struct model_error *locMod1, *locMod2; 
  enum whichModel {LEFT_MODEL=0, RIGHT_MODEL=1};
  void infoModel(struct model_error *model, int id);
};


#endif
