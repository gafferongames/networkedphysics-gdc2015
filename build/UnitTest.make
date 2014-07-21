# GNU Make project makefile autogenerated by Premake
ifndef config
  config=debug64
endif

ifndef verbose
  SILENT = @
endif

ifndef CC
  CC = gcc
endif

ifndef CXX
  CXX = g++
endif

ifndef AR
  AR = ar
endif

ifndef RESCOMP
  ifdef WINDRES
    RESCOMP = $(WINDRES)
  else
    RESCOMP = windres
  endif
endif

ifeq ($(config),debug64)
  OBJDIR     = obj/x64/Debug/UnitTest
  TARGETDIR  = ../bin
  TARGET     = $(TARGETDIR)/UnitTest
  DEFINES   += 
  INCLUDES  += -I../include -I..
  CPPFLAGS  += -MMD -MP $(DEFINES) $(INCLUDES)
  CFLAGS    += $(CPPFLAGS) $(ARCH) -g -Wall -msse2 -ffast-math -m64 -std=c++11 -stdlib=libc++
  CXXFLAGS  += $(CFLAGS) -fno-rtti -fno-exceptions
  LDFLAGS   += -L../lib -m64 -L/usr/lib64
  RESFLAGS  += $(DEFINES) $(INCLUDES) 
  LIBS      += ../lib/libprotocol.a
  LDDEPS    += ../lib/libprotocol.a
  LINKCMD    = $(CXX) -o $(TARGET) $(OBJECTS) $(RESOURCES) $(ARCH) $(LIBS) $(LDFLAGS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
endif

ifeq ($(config),release64)
  OBJDIR     = obj/x64/Release/UnitTest
  TARGETDIR  = ../bin
  TARGET     = $(TARGETDIR)/UnitTest
  DEFINES   += -DNDEBUG
  INCLUDES  += -I../include -I..
  CPPFLAGS  += -MMD -MP $(DEFINES) $(INCLUDES)
  CFLAGS    += $(CPPFLAGS) $(ARCH) -g -Wall -msse2 -ffast-math -O3 -m64 -std=c++11 -stdlib=libc++
  CXXFLAGS  += $(CFLAGS) -fno-rtti -fno-exceptions
  LDFLAGS   += -L../lib -m64 -L/usr/lib64
  RESFLAGS  += $(DEFINES) $(INCLUDES) 
  LIBS      += ../lib/libprotocol.a
  LDDEPS    += ../lib/libprotocol.a
  LINKCMD    = $(CXX) -o $(TARGET) $(OBJECTS) $(RESOURCES) $(ARCH) $(LIBS) $(LDFLAGS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
endif

ifeq ($(config),debug32)
  OBJDIR     = obj/x32/Debug/UnitTest
  TARGETDIR  = ../bin
  TARGET     = $(TARGETDIR)/UnitTest
  DEFINES   += 
  INCLUDES  += -I../include -I..
  CPPFLAGS  += -MMD -MP $(DEFINES) $(INCLUDES)
  CFLAGS    += $(CPPFLAGS) $(ARCH) -g -Wall -msse2 -ffast-math -m32 -std=c++11 -stdlib=libc++
  CXXFLAGS  += $(CFLAGS) -fno-rtti -fno-exceptions
  LDFLAGS   += -L../lib -m32 -L/usr/lib32
  RESFLAGS  += $(DEFINES) $(INCLUDES) 
  LIBS      += ../lib/libprotocol.a
  LDDEPS    += ../lib/libprotocol.a
  LINKCMD    = $(CXX) -o $(TARGET) $(OBJECTS) $(RESOURCES) $(ARCH) $(LIBS) $(LDFLAGS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
endif

ifeq ($(config),release32)
  OBJDIR     = obj/x32/Release/UnitTest
  TARGETDIR  = ../bin
  TARGET     = $(TARGETDIR)/UnitTest
  DEFINES   += -DNDEBUG
  INCLUDES  += -I../include -I..
  CPPFLAGS  += -MMD -MP $(DEFINES) $(INCLUDES)
  CFLAGS    += $(CPPFLAGS) $(ARCH) -g -Wall -msse2 -ffast-math -O3 -m32 -std=c++11 -stdlib=libc++
  CXXFLAGS  += $(CFLAGS) -fno-rtti -fno-exceptions
  LDFLAGS   += -L../lib -m32 -L/usr/lib32
  RESFLAGS  += $(DEFINES) $(INCLUDES) 
  LIBS      += ../lib/libprotocol.a
  LDDEPS    += ../lib/libprotocol.a
  LINKCMD    = $(CXX) -o $(TARGET) $(OBJECTS) $(RESOURCES) $(ARCH) $(LIBS) $(LDFLAGS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
endif

OBJECTS := \
	$(OBJDIR)/UnitTest.o \
	$(OBJDIR)/TestAddress.o \
	$(OBJDIR)/TestBitPacker.o \
	$(OBJDIR)/TestBlock.o \
	$(OBJDIR)/TestBSDSocket.o \
	$(OBJDIR)/TestClientServer.o \
	$(OBJDIR)/TestCommon.o \
	$(OBJDIR)/TestConnection.o \
	$(OBJDIR)/TestDataBlockSendAndReceive.o \
	$(OBJDIR)/TestDNSResolver.o \
	$(OBJDIR)/TestFoundation.o \
	$(OBJDIR)/TestMessageFactory.o \
	$(OBJDIR)/TestPacketFactory.o \
	$(OBJDIR)/TestReliableMessageChannel.o \
	$(OBJDIR)/TestSlidingWindow.o \
	$(OBJDIR)/TestStream.o \

RESOURCES := \

SHELLTYPE := msdos
ifeq (,$(ComSpec)$(COMSPEC))
  SHELLTYPE := posix
endif
ifeq (/bin,$(findstring /bin,$(SHELL)))
  SHELLTYPE := posix
endif

.PHONY: clean prebuild prelink

all: $(TARGETDIR) $(OBJDIR) prebuild prelink $(TARGET)
	@:

$(TARGET): $(GCH) $(OBJECTS) $(LDDEPS) $(RESOURCES)
	@echo Linking UnitTest
	$(SILENT) $(LINKCMD)
	$(POSTBUILDCMDS)

$(TARGETDIR):
	@echo Creating $(TARGETDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(TARGETDIR)
else
	$(SILENT) mkdir $(subst /,\\,$(TARGETDIR))
endif

$(OBJDIR):
	@echo Creating $(OBJDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(OBJDIR)
else
	$(SILENT) mkdir $(subst /,\\,$(OBJDIR))
endif

clean:
	@echo Cleaning UnitTest
ifeq (posix,$(SHELLTYPE))
	$(SILENT) rm -f  $(TARGET)
	$(SILENT) rm -rf $(OBJDIR)
else
	$(SILENT) if exist $(subst /,\\,$(TARGET)) del $(subst /,\\,$(TARGET))
	$(SILENT) if exist $(subst /,\\,$(OBJDIR)) rmdir /s /q $(subst /,\\,$(OBJDIR))
endif

prebuild:
	$(PREBUILDCMDS)

prelink:
	$(PRELINKCMDS)

ifneq (,$(PCH))
$(GCH): $(PCH)
	@echo $(notdir $<)
ifeq (posix,$(SHELLTYPE))
	-$(SILENT) cp $< $(OBJDIR)
else
	$(SILENT) xcopy /D /Y /Q "$(subst /,\,$<)" "$(subst /,\,$(OBJDIR))" 1>nul
endif
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
endif

$(OBJDIR)/UnitTest.o: ../tests/UnitTest.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestAddress.o: ../tests/TestAddress.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestBitPacker.o: ../tests/TestBitPacker.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestBlock.o: ../tests/TestBlock.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestBSDSocket.o: ../tests/TestBSDSocket.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestClientServer.o: ../tests/TestClientServer.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestCommon.o: ../tests/TestCommon.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestConnection.o: ../tests/TestConnection.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestDataBlockSendAndReceive.o: ../tests/TestDataBlockSendAndReceive.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestDNSResolver.o: ../tests/TestDNSResolver.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestFoundation.o: ../tests/TestFoundation.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestMessageFactory.o: ../tests/TestMessageFactory.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestPacketFactory.o: ../tests/TestPacketFactory.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestReliableMessageChannel.o: ../tests/TestReliableMessageChannel.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestSlidingWindow.o: ../tests/TestSlidingWindow.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/TestStream.o: ../tests/TestStream.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(CXXFLAGS) -o "$@" -MF $(@:%.o=%.d) -c "$<"

-include $(OBJECTS:%.o=%.d)
