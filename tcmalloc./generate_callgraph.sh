#!/bin/bash

# 生成可视化调用图 - kcachegrind 的替代方案
# 使用 gprof2dot + graphviz 生成 PNG 图片

echo "🎨 生成可视化调用图"
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
    exit 1
fi

echo "📁 使用文件: $CALLGRIND_FILE"
echo ""

# 检查依赖
NEED_INSTALL=0

if ! command -v dot &> /dev/null; then
    echo "⚠️  graphviz 未安装"
    NEED_INSTALL=1
fi

if ! command -v gprof2dot &> /dev/null; then
    echo "⚠️  gprof2dot 未安装"
    NEED_INSTALL=1
fi

if [ $NEED_INSTALL -eq 1 ]; then
    echo ""
    echo "📦 正在安装依赖..."
    echo ""
    
    # 安装 graphviz
    if ! command -v dot &> /dev/null; then
        echo "安装 graphviz..."
        sudo apt-get update
        sudo apt-get install -y graphviz
    fi
    
    # 安装 gprof2dot
    if ! command -v gprof2dot &> /dev/null; then
        echo "安装 gprof2dot..."
        pip3 install gprof2dot --user 2>&1 | grep -v "WARNING"
        
        # 添加到 PATH
        export PATH="$HOME/.local/bin:$PATH"
    fi
fi

echo ""
echo "✅ 依赖已就绪"
echo ""

# 生成调用图
OUTPUT_PNG="$LATEST_DIR/callgraph.png"
OUTPUT_SVG="$LATEST_DIR/callgraph.svg"

echo "🎨 生成调用图 (PNG)..."
gprof2dot -f callgrind "$CALLGRIND_FILE" | \
    dot -Tpng -o "$OUTPUT_PNG"

if [ $? -eq 0 ]; then
    echo "✅ PNG 图已生成: $OUTPUT_PNG"
else
    echo "❌ PNG 生成失败"
fi

echo ""
echo "🎨 生成调用图 (SVG，可缩放)..."
gprof2dot -f callgrind "$CALLGRIND_FILE" | \
    dot -Tsvg -o "$OUTPUT_SVG"

if [ $? -eq 0 ]; then
    echo "✅ SVG 图已生成: $OUTPUT_SVG"
else
    echo "❌ SVG 生成失败"
fi

echo ""
echo "📊 生成简化版（只显示主要函数）..."
OUTPUT_SIMPLE="$LATEST_DIR/callgraph_simple.png"
gprof2dot -f callgrind -n 2.0 -e 1.0 "$CALLGRIND_FILE" | \
    dot -Tpng -o "$OUTPUT_SIMPLE"

if [ $? -eq 0 ]; then
    echo "✅ 简化图已生成: $OUTPUT_SIMPLE"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🎉 完成！"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "生成的文件："
ls -lh "$LATEST_DIR"/callgraph*.png "$LATEST_DIR"/*.svg 2>/dev/null
echo ""
echo "查看图片："
echo "  xdg-open $OUTPUT_PNG"
echo "  xdg-open $OUTPUT_SVG       # SVG 可无限缩放"
echo "  xdg-open $OUTPUT_SIMPLE    # 简化版"
echo ""

# 自动打开
read -p "是否立即查看图片？[Y/n] " answer
answer=${answer:-Y}

if [[ "$answer" =~ ^[Yy]$ ]]; then
    echo "打开图片..."
    xdg-open "$OUTPUT_PNG" 2>/dev/null &
    sleep 1
    xdg-open "$OUTPUT_SIMPLE" 2>/dev/null &
    echo "✅ 已打开图片查看器"
fi

echo ""
echo "💡 提示："
echo "- callgraph.png: 完整调用图"
echo "- callgraph_simple.png: 只显示占比 > 2% 的函数"
echo "- callgraph.svg: 矢量图，可无限缩放"
