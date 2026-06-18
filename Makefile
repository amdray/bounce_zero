TARGET = Bounce
OBJS = src/main.o src/graphics.o src/input.o src/game.o src/physics.o src/level.o src/png.o src/cbmf.o src/cbmf_psp.o src/cbmf_fonts.o src/menu.o src/tile_table.o src/sound.o src/save.o src/local.o src/local_extra.o src/splash.o

INCDIR = src/
CFLAGS = -O2 -G0 -Wall -Wextra -Wshadow -Wfloat-conversion -Werror=implicit-function-declaration -std=c99 -MMD -MP -Isrc
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspgu -lpspgum -lpspdisplay -lpspge -lpspctrl -lpspaudiolib -lpspaudio -lz

# ---- Настройки PSP EBOOT ----
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Bounce
PSP_EBOOT_ICON  = ICON0.PNG
PSP_EBOOT_PIC1  = PIC1.PNG
SFOFLAGS += -d DISC_ID=BZERO0001

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# Include auto-generated dependency files
-include $(OBJS:.o=.d)

# ---- Авто-сборка release + backup после компиляции ----
TIMESTAMP   := $(shell date +"%Y-%m-%d_%H-%M-%S")
BACKUP_DIR  := backup
BACKUP_FILE := $(BACKUP_DIR)/$(TIMESTAMP).src.zip
RELEASE_DIR = ./release

.PHONY: default
default: $(EXTRA_TARGETS)
	@echo "Creating release folder..."
	@mkdir -p $(RELEASE_DIR)
	@mv -f EBOOT.PBP $(RELEASE_DIR)/
	@rsync -ru --size-only icons/  $(RELEASE_DIR)/icons/
	@rsync -ru --size-only levels/ $(RELEASE_DIR)/levels/
	@rsync -ru --size-only sounds/ $(RELEASE_DIR)/sounds/
	@rsync -ru --size-only lang/   $(RELEASE_DIR)/lang/
	@rsync -ru --size-only fonts/  $(RELEASE_DIR)/fonts/
	@rm -f Bounce.elf
	@mkdir -p $(BACKUP_DIR)
	@zip -rq "$(BACKUP_FILE)" Makefile icons levels lang fonts src \
	    -x "src/*.o" -x "src/*.d"
	@echo "Done: release/ + backup/$(TIMESTAMP).src.zip"

.DEFAULT_GOAL := default
