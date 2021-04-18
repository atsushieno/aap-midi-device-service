
ABIS_SIMPLE= x86 x86_64 armeabi-v7a arm64-v8a

all: build-all

build-all: \
	build-aap-core \
	build-java

build-aap-core:
	cd external/android-audio-plugin-framework && make all-no-desktop

## Build utility

build-java:
	ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) ./gradlew build publishToMavenLocal
 
