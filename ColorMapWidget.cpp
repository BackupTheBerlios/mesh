/* $Id: ColorMapWidget.cpp,v 1.4 2001/06/13 14:13:37 jacquet Exp $ */
#include <ColorMapWidget.h>

/***************************************************************************/
/*        definition de la classe ColorMapWidget                           */
/***************************************************************************/



ColorMapWidget::ColorMapWidget(double dmoymin, double dmoymax,QWidget *parent, const char *name)
  :QWidget(parent,name)
{
  setMinimumSize( 70, 512 );
  setMaximumSize( 70, 512 );
  setBackgroundColor( black );
  colormap=HSVtoRGB();
  dmax=dmoymax;
  dmin=dmoymin;
//   printf("%f %f",dmoymin,dmoymax);
}

void ColorMapWidget::paintEvent(QPaintEvent *)
{
  double res;
  QPainter p;
  p.begin(this);
  
  
  QString n;

  for(int i=0; i<8;i++){
    p.setBrush(QColor((int)(255*colormap[7-i][0]),(int)(255*colormap[7-i][1]),(int)(255*colormap[7-i][2])));
    p.setPen(QColor((int)floor(255*colormap[7-i][0]),(int)floor(255*colormap[7-i][1]),(int)floor(255*colormap[7-i][2])));
    p.drawRect(10,i*64,20,64);
    p.setPen(white);
    p.setFont(QFont( "courier", 8) );
    res= dmax-(dmax-dmin)*i/7;
    n.sprintf( "%5f",res);
    p.drawText(35,i*64+64,n);
  }
  p.end();
}
