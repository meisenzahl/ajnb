TARGET = ajnb

SRC_DIR = src
INC_DIR = inc
OBJ_DIR = obj
BIN_DIR = bin

SRC_EXT = cc
OBJ_EXT = o

SOURCES = $(shell find $(SRC_DIR) -type f)
OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(SOURCES:.$(SRC_EXT)=.$(OBJ_EXT)))

CXXFLAGS = -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX

WORKSPACE = $(shell pwd)

INC = -I$(INC_DIR) \
		-I$(ALLJOYN_DIST)/cpp/inc \
		-I$(ALLJOYN_DIST)/notification/inc \
		-I$(ALLJOYN_DIST)/services_common/inc
LIBS = -lstdc++ -lcrypto -lpthread -lrt \
		-L$(ALLJOYN_DIST)/cpp/lib -lalljoyn \
		-L$(ALLJOYN_DIST)/notification/lib -lalljoyn_notification \
		-L$(ALLJOYN_DIST)/services_common/lib -lalljoyn_services_common

.PHONY: clean mrproper

default: $(TARGET)

$(OBJ_DIR)/%.$(OBJ_EXT): $(SRC_DIR)/%.$(SRC_EXT)
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INC) -c -o $@ `pkg-config --cflags --libs libnotify` $<

$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $(BIN_DIR)/$(TARGET) $^ `pkg-config --cflags --libs libnotify` $(LIBS)

clean:
	@$(RM) -rf $(BIN_DIR)

mrproper:
	@$(RM) -rf $(BIN_DIR) $(OBJ_DIR)
