# Local Fedora/Linux build and desktop installation, like ggaze.
# make / make run / make test / make install / make uninstall
# Installs into ~/.local by default; override PREFIX for another location.
PREFIX      ?= $(HOME)/.local
BUILDDIR    ?= build
JOBS        ?= $(shell nproc)
CMAKE_FLAGS ?=

.PHONY: all configure build run test install uninstall refresh-desktop deps-fedora clean

all: build

configure:
	cmake -S . -B "$(BUILDDIR)" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(PREFIX)" $(CMAKE_FLAGS)

build: configure
	cmake --build "$(BUILDDIR)" --target gunrunners --parallel "$(JOBS)"

run: build
	"$(BUILDDIR)/gunrunners" $(ARGS)

test: configure
	cmake --build "$(BUILDDIR)" --parallel "$(JOBS)"
	ctest --test-dir "$(BUILDDIR)" --parallel "$(JOBS)" --output-on-failure

install: build
	cmake --install "$(BUILDDIR)"
	$(MAKE) refresh-desktop
	@echo "Installed Gunrunners into $(PREFIX). Search for Gunrunners in your application launcher."

uninstall:
	cmake -DBUILD_DIR="$(abspath $(BUILDDIR))" -P cmake/Uninstall.cmake
	$(MAKE) refresh-desktop

refresh-desktop:
	@if command -v update-desktop-database >/dev/null 2>&1 && [ -d "$(DESTDIR)$(PREFIX)/share/applications" ]; then \
		update-desktop-database "$(DESTDIR)$(PREFIX)/share/applications"; \
	fi
	@if command -v gtk-update-icon-cache >/dev/null 2>&1 && [ -d "$(DESTDIR)$(PREFIX)/share/icons/hicolor" ]; then \
		gtk-update-icon-cache -f -t "$(DESTDIR)$(PREFIX)/share/icons/hicolor"; \
	fi

deps-fedora:
	sudo dnf install gcc-c++ make cmake pkgconf-pkg-config SDL2-devel cairo-devel libcurl-devel dejavu-sans-fonts desktop-file-utils

clean:
	cmake --build "$(BUILDDIR)" --target clean
