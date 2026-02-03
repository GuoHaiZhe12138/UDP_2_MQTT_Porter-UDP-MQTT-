# 1. 定义编译器
CXX = g++

# 2. 编译选项
# -std=c++17: 使用 C++17 标准
# -Wall: 显示所有警告
# -I./include: 明确告诉编译器去 include 文件夹找头文件 (解决你刚才的报错)
# -O2: 开启二级优化
CXXFLAGS = -std=c++17 -Wall -I./include -O2

# 3. 链接选项 (库)
# -lmosquitto: 链接 MQTT 库
# -lpthread: 链接线程库
# -latomic: 链接原子操作库 (Ubuntu 18.04 必需)
LDFLAGS = -lmosquitto -lpthread -latomic

# 4. 定义目标文件和源文件
TARGET = sentinel_gate
SRCS = src/SentinelGate.cpp examples/main.cpp
# 将 .cpp 文件名替换为 .o 文件名
OBJS = $(SRCS:.cpp=.o)

# 5. 默认任务
all: $(TARGET)

# 6. 链接过程：将所有的 .o 文件链接成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# 7. 编译过程：将每个 .cpp 文件编译成 .o 目标文件
# % 是通配符，$< 代表源文件，$@ 代表目标文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 8. 清理任务
clean:
	rm -f $(OBJS) $(TARGET)

# 9. 伪目标，防止文件夹里有叫 clean 的文件导致冲突
.PHONY: all clean