# $Id: Makefile,v 1.10 2001/08/07 08:31:41 aspert Exp $

# Default compiler for C and C++ (CPP is normally the C preprocessor)
CC = gcc
CXX = g++

BINDIR = ./bin
LIBDIR = ./lib
OBJDIR = ./obj
LIB3DDIR = ./lib3d
# QTDIR should come from the environment
ifndef QTDIR
$(error The QTDIR envirnoment variable is not defined. Define it as the path \
	to the QT installation directory)
endif
MOC = $(QTDIR)/bin/moc

# Extra compiler flags (optimization, profiling, debug, etc.)
XTRA_CFLAGS = -g -O2 -ansi -march=i686
XTRA_CXXFLAGS = -g -O2 -ansi
XTRA_LDFLAGS = -g

# Source files and executable name
VIEWER_EXE := $(BINDIR)/viewer
VIEWER_C_SRCS := $(wildcard *.c)
VIEWER_CXX_SRCS := $(wildcard *.cpp)
VIEWER_MOC_SRCS := RawWidget.h ScreenWidget.h InitWidget.h
LIB3D_C_SRCS = 3dmodel_io.c normals.c geomutils.c

# Compiler and linker flags
INCFLAGS = -I$(LIB3DDIR)/include -I.
QTINCFLAGS = -I$(QTDIR)/include
GLINCFLAGS = -I/usr/X11R6/include

# Libraries and search path for final linking
LDLIBS = -lqt -lGL -lGLU -lXmu -lXext -lX11 -lm
LOADLIBES = -L$(QTDIR)/lib -L/usr/X11R6/lib
LDFLAGS =

# C and C++ warning flags
WARN_CFLAGS = -pedantic -Wall -W -Winline -Wmissing-prototypes -Wstrict-prototypes -Wnested-externs -Wshadow -Waggregate-return
WARN_CXXFLAGS = -pedantic -Wall -W -Wmissing-prototypes

# Preprocessor flags
CPPFLAGS = $(INCFLAGS) -D_METRO

# Construct basic compiler flags
CFLAGS = $(WARN_CFLAGS) $(XTRA_CFLAGS)
CXXFLAGS = $(WARN_CXXFLAGS) $(XTRA_CXXFLAGS)
LDFLAGS = $(XTRA_LDFLAGS)

# Automatically derived file names
MOC_CXX_SRCS = $(addprefix moc_,$(VIEWER_MOC_SRCS:.h=.cpp))
VIEWER_OBJS = $(addprefix $(OBJDIR)/, $(VIEWER_C_SRCS:.c=.o) \
	$(VIEWER_CXX_SRCS:.cpp=.o) $(MOC_CXX_SRCS:.cpp=.o))
LIB3D_OBJS = $(addprefix $(OBJDIR)/,$(LIB3D_C_SRCS:.c=.o))
LIB3D_SLIB = $(addprefix $(LIBDIR)/,lib3d.a)

#
# Targets
#

# Main targets
default: $(VIEWER_EXE)

all: dirs $(VIEWER_EXE)

clean: 
	-rm -f deps $(OBJDIR)/*.o $(BINDIR)/* $(LIBDIR)/*

# Executable
$(VIEWER_EXE): $(VIEWER_OBJS) $(LIB3D_SLIB)
	$(CXX) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

# LIB3D static library (only what we need of lib3d)
# GNU make automatic rule for archives will be used here
$(LIB3D_SLIB): $(LIB3D_SLIB)($(LIB3D_OBJS))

# QT/OpenGL GUI (C++)
$(OBJDIR)/%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $(QTINCFLAGS) $(GLINCFLAGS) $^ -o $@

# Produce QT moc sources
moc_%.cpp: %.h
	$(MOC) $^ -o $@

# Error computing functions
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $^ -o $@

# lib3d sources
$(LIB3D_OBJS): $(OBJDIR)/%.o : $(LIB3DDIR)/src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $^ -o $@

#
# Automatic dependency
#

ifneq ($(findstring clean,$(MAKECMDGOALS)),clean)
include deps
endif

deps:: $(VIEWER_C_SRCS) $(addprefix $(LIB3DDIR)/src/,$(LIB3D_C_SRCS))
	$(CC) -M $(CPPFLAGS) $^ >> deps
deps:: $(VIEWER_CXX_SRCS)
	$(CXX) -M $(CPPFLAGS) $(QTINCFLAGS) $(GLINCFLAGS) $^ >> deps


#
# Directories
#
dirs : libdir bindir objdir

libdir : 
	-[ -d $(LIBDIR) ] || mkdir $(LIBDIR)

bindir : 
	-[ -d $(BINDIR) ] || mkdir $(BINDIR)

objdir :
	-[ -d $(OBJDIR) ] || mkdir $(OBJDIR)

# Targets which are not real files
.PHONY: default all dirs clean libdir bindir objdir
