#!/bin/bash

# kcachegrind 启动修复脚本
# 解决 snap 库冲突问题

echo "🔧 启动 kcachegrind (修复版)"
echo ""

# 查找最新的分析结果
LATEST_DIR=$(ls -td valgrind_results_* 2>/dev/null | head -n 1)

if [ -z "$LATEST_DIR" ]; then
    echo "❌ 未找到分析结果"
    echo "请先运行: ./valgrind_analysis.sh"
    exit 1
fi

CALLGRIND_FILE="$LATEST_DIR/callgrind.out"

if [ ! -f "$CALLGRIND_FILE" ]; then
    echo "❌ 未找到 callgrind.out 文件"
    echo "请先运行完整的 Valgrind 分析"
    exit 1
fi

echo "📁 使用文件: $CALLGRIND_FILE"
echo ""

# 方法1: 清理 snap 环境变量
echo "尝试方法 1: 清理环境变量后启动..."
env -u LD_LIBRARY_PATH \
    -u LD_PRELOAD \
    -u SNAP \
    -u SNAP_NAME \
    -u SNAP_REVISION \
    kcachegrind "$CALLGRIND_FILE" 2>&1 &

KCACHE_PID=$!
sleep 2

# 检查是否成功启动
if ps -p $KCACHE_PID > /dev/null 2>&1; then
    echo "✅ kcachegrind 已成功启动 (PID: $KCACHE_PID)"
    echo ""
    echo "💡 提示: 如果窗口没有出现，可能被挡住了"
    exit 0
else
    echo "⚠️ 方法 1 失败，尝试其他方案..."
    echo ""
fi

# 方法2: 使用 qcachegrind (Qt5 版本)
if command -v qcachegrind &> /dev/null; then
    echo "尝试方法 2: 使用 qcachegrind..."
    qcachegrind "$CALLGRIND_FILE" &
    echo "✅ qcachegrind 已启动"
    exit 0
fi

# 方法3: 提供命令行替代方案
echo "❌ 图形界面工具无法启动"
echo ""
echo "📊 使用命令行查看热点函数:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

callgrind_annotate "$CALLGRIND_FILE" 2>/dev/null | head -n 50

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "💡 替代方案:"
echo ""
echo "1️⃣  命令行查看完整报告:"
echo "   callgrind_annotate $CALLGRIND_FILE | less"
echo ""
echo "2️⃣  查看特定函数:"
echo "   callgrind_annotate $CALLGRIND_FILE | grep 'ConcurrentAlloc'"
echo ""
echo "3️⃣  安装 qcachegrind (替代工具):"
echo "   sudo apt-get install qcachegrind"
echo ""
echo "4️⃣  在另一台机器上查看:"
echo "   - 复制 callgrind.out 文件到其他机器"
echo "   - 使用那台机器的 kcachegrind 打开"
echo ""
echo "5️⃣  使用在线工具:"
echo "   https://kcachegrind.github.io/html/Home.html"
