/* $Id: rawview_grab.c,v 1.2 2002/09/12 11:55:41 aspert Exp $ */

#include <rawview_misc.h>

/* *********************************************************** */
/* This is a _basic_ frame grabber -> copy all the RGB buffers */
/* and put'em into grab$$$.ppm where $$$ is [000; 999]         */
/* This may kill the X server under undefined conditions       */
/* This function is called by pressing the 'F6' key            */
/* *********************************************************** */
void frame_grab(struct gl_render_context *gl_ctx) {
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
  sprintf(filename,"grab%03d.ppm", gl_ctx->grab_number++);

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


/* ***************************** */
/* Writes the frame to a PS file */
/* ***************************** */
void ps_grab(struct gl_render_context *gl_ctx, 
             struct display_lists_indices *dl_idx, int flag) {
  int bufsize = 0, state = GL2PS_OVERFLOW;
  char filename[13];
  FILE *ps_file;

  sprintf(filename, "psgrab%03d.ps", gl_ctx->ps_number);
  ps_file = fopen(filename, "w");
  if (ps_file == NULL)
    fprintf(stderr, "Unable to open PS outfile %s\n", filename);
  else {


    if (flag == 1) {
      glClearColor(1.0, 1.0, 1.0, 0.0);
      gl_ctx->ps_rend = 1;
    }
    else { /* If we are rendering a negative, we have to switch also
              rendering colors */
      glClearColor(0.0, 0.0, 0.0, 0.0);
      gl_ctx->ps_rend = 2;
    }
    while (state == GL2PS_OVERFLOW) {
      bufsize += 1024*1024;
      gl2psBeginPage("PS Grab", "LaTeX", GL2PS_PS, GL2PS_SIMPLE_SORT, 
		     GL2PS_SIMPLE_LINE_OFFSET, 
		     GL_RGBA, 0, NULL, bufsize, ps_file, 
		     filename);
    
      display_wrapper(gl_ctx, dl_idx);
      state = gl2psEndPage();
    }
    gl_ctx->ps_number++;
    printf("Buffer for PS grab was %d bytes\n", bufsize);
    gl_ctx->ps_rend = 0;
    glClearColor(0.0, 0.0, 0.0, 0.0);
    fclose(ps_file);
  }
}
