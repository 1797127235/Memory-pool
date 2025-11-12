#!/bin/bash

# 查看最新的性能分析结果

echo "======================================"
echo "  性能分析结果查看器"
echo "======================================"

# 查找最新的结果目录
LATEST_DIR=$(ls -td valgrind_results_* 2>/dev/null | head -n 1)

if [ -z "$LATEST_DIR" ]; then
    echo "❌ 未找到分析结果"
    echo ""
    echo "请先运行以下命令之一："
    echo "  ./quick_perf.sh          # 快速分析"
    echo "  ./valgrind_analysis.sh   # 详细分析"
    exit 1
fi

echo "📁 最新结果目录: $LATEST_DIR"
echo ""

# 菜单
echo "请选择要查看的内容："
echo ""
echo "  1) 📊 HTML 摘要报告（浏览器）"
echo "  2) 📝 文本摘要"
echo "  3) 🔍 内存泄漏检测"
echo "  4) 🔥 热点函数 Top 20"
echo "  5) 💾 Cache 性能统计"
echo "  6) 📈 堆内存使用"
echo "  7) 🎨 可视化（kcachegrind）"
echo "  8) �️  生成调用图（PNG/SVG）"
echo "  9) �📂 打开结果目录"
echo "  a) 📋 列出所有文件"
echo "  0) 🚪 退出"
echo ""
read -p "请输入选项 [0-9,a]: " choice

case $choice in
    1)
        echo "正在打开 HTML 报告..."
        if [ -f "$LATEST_DIR/summary.html" ]; then
            xdg-open "$LATEST_DIR/summary.html" 2>/dev/null || \
            firefox "$LATEST_DIR/summary.html" 2>/dev/null || \
            echo "请手动打开: $LATEST_DIR/summary.html"
        else
            echo "❌ 未找到 summary.html"
        fi
        ;;
    2)
        echo "======================================"
        echo "  文本摘要"
        echo "======================================"
        cat "$LATEST_DIR/README.txt" 2>/dev/null || echo "❌ 未找到 README.txt"
        ;;
    3)
        echo "======================================"
        echo "  内存泄漏检测结果"
        echo "======================================"
        if [ -f "$LATEST_DIR/memcheck.txt" ]; then
            echo ""
            grep -A 15 "LEAK SUMMARY" "$LATEST_DIR/memcheck.txt"
            echo ""
            echo "详细信息: less $LATEST_DIR/memcheck.txt"
        else
            echo "❌ 未找到 memcheck.txt"
        fi
        ;;
    4)
        echo "======================================"
        echo "  热点函数 Top 20"
        echo "======================================"
        if [ -f "$LATEST_DIR/callgrind.out" ]; then
            callgrind_annotate "$LATEST_DIR/callgrind.out" 2>/dev/null | head -n 50
        else
            echo "❌ 未找到 callgrind.out"
        fi
        ;;
    5)
        echo "======================================"
        echo "  Cache 性能统计"
        echo "======================================"
        if [ -f "$LATEST_DIR/cachegrind_report.txt" ]; then
            echo ""
            grep -E "I refs|I1 miss|D refs|D1 miss|LL.*miss" "$LATEST_DIR/cachegrind_report.txt" | head -n 30
            echo ""
            echo "详细信息: less $LATEST_DIR/cachegrind_report.txt"
        else
            echo "❌ 未找到 cachegrind_report.txt"
        fi
        ;;
    6)
        echo "======================================"
        echo "  堆内存使用分析"
        echo "======================================"
        if [ -f "$LATEST_DIR/massif_report.txt" ]; then
            head -n 100 "$LATEST_DIR/massif_report.txt"
            echo ""
            echo "详细信息: less $LATEST_DIR/massif_report.txt"
        else
            echo "❌ 未找到 massif_report.txt"
        fi
        ;;
    7)
        echo "启动 kcachegrind..."
        if command -v kcachegrind &> /dev/null; then
            if [ -f "$LATEST_DIR/callgrind.out" ]; then
                ./fix_kcachegrind.sh
            else
                echo "❌ 未找到 callgrind.out"
            fi
        else
            echo "❌ kcachegrind 未安装"
            echo "请运行: sudo apt-get install kcachegrind"
        fi
        ;;
    8)
        echo "生成可视化调用图..."
        ./generate_callgraph.sh
        ;;
    9)
        echo "打开文件管理器..."
        xdg-open "$LATEST_DIR" 2>/dev/null || \
        nautilus "$LATEST_DIR" 2>/dev/null || \
        echo "请手动打开: $LATEST_DIR"
        ;;
    a|A)
        echo "======================================"
        echo "  结果文件列表"
        echo "======================================"
        ls -lh "$LATEST_DIR/"
        echo ""
        echo "目录路径: $LATEST_DIR"
        ;;
    0)
        echo "再见！"
        exit 0
        ;;
    *)
        echo "❌ 无效选项"
        exit 1
        ;;
esac

echo ""
echo "======================================"
echo "  操作完成"
echo "======================================"
echo ""
echo "提示: 重新运行此脚本可查看其他内容"
echo "      ./view_results.sh"
