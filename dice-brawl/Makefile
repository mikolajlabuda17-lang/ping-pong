APP := dice-brawl
BIN := bin/$(APP)

.PHONY: all build clean package zip

all: build

build:
	GOOS=linux GOARCH=amd64 go build -trimpath -ldflags "-s -w" -o $(BIN) ./...

clean:
	rm -rf bin dist

package: build
	mkdir -p dist/$(APP)/installer dist/$(APP)/bin
	cp -a installer/install.sh dist/$(APP)/installer/
	cp -a installer/installer.desktop dist/$(APP)/installer/
	cp -a bin/$(APP) dist/$(APP)/bin/
	# optional icon placeholder
	printf "\x89PNG\r\n\x1a\n" > dist/$(APP)/icon.png || true
	# archive
	cd dist && tar -czf $(APP).tar.gz $(APP)
	@echo "Wygenerowano: dist/$(APP).tar.gz"

zip: build
	mkdir -p dist/$(APP)/installer dist/$(APP)/bin
	cp -a installer/install.sh dist/$(APP)/installer/
	cp -a installer/installer.desktop dist/$(APP)/installer/
	cp -a bin/$(APP) dist/$(APP)/bin/
	printf "\x89PNG\r\n\x1a\n" > dist/$(APP)/icon.png || true
	cd dist && zip -qr $(APP).zip $(APP)
	@echo "Wygenerowano: dist/$(APP).zip"