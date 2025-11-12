#!/bin/bash

# Valgrind 专用性能分析脚本
# 适用于 perf 不可用的情况

set -e

echo "======================================"
echo "  Valgrind 性能分析"
echo "======================================"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 检查 Valgrind
if ! command -v valgrind &> /dev/null; then
    echo "错误: Valgrind 未安装"
    echo "请运行: sudo apt-get install valgrind"
    exit 1
fi

echo -e "${GREEN}✓ Valgrind 已安装${NC}"

# 编译
echo -e "\n${YELLOW}[1/6] 编译调试版本...${NC}"
make bench_debug
echo -e "${GREEN}✓ 编译完成${NC}"

# 创建结果目录
RESULT_DIR="valgrind_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULT_DIR"
echo -e "${GREEN}✓ 结果目录: $RESULT_DIR${NC}"

# 测试参数
SMALL_N=10000    # Valgrind 会很慢，用较小的数据集
LARGE_N=100000   # 需要更详细分析时使用

echo -e "\n${YELLOW}[2/6] Memcheck - 内存错误和泄漏检测${NC}"
echo "这可能需要 1-2 分钟..."
valgrind --tool=memcheck \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --verbose \
    --log-file="$RESULT_DIR/memcheck.txt" \
    ./bench_debug $SMALL_N

echo -e "${GREEN}✓ 内存检测完成${NC}"
echo "  查看摘要:"
grep -A 10 "LEAK SUMMARY" "$RESULT_DIR/memcheck.txt" || echo "  无泄漏信息"

echo -e "\n${YELLOW}[3/6] Cachegrind - Cache 性能分析${NC}"
echo "分析 CPU cache 命中率，需要 2-3 分钟..."
valgrind --tool=cachegrind \
    --cachegrind-out-file="$RESULT_DIR/cachegrind.out" \
    ./bench_debug $SMALL_N 2>&1 | tee "$RESULT_DIR/cachegrind.log"

# 生成注释报告
cg_annotate "$RESULT_DIR/cachegrind.out" > "$RESULT_DIR/cachegrind_report.txt"
echo -e "${GREEN}✓ Cache 分析完成${NC}"

# 显示 cache 摘要
echo "  Cache 统计摘要:"
grep -E "I1|D1|LL" "$RESULT_DIR/cachegrind_report.txt" | head -n 20

echo -e "\n${YELLOW}[4/6] Callgrind - 调用图和性能分析${NC}"
echo "分析函数调用关系和时间分布，需要 3-5 分钟..."
valgrind --tool=callgrind \
    --callgrind-out-file="$RESULT_DIR/callgrind.out" \
    ./bench_debug $SMALL_N 2>&1 | tee "$RESULT_DIR/callgrind.log"

# 生成报告
callgrind_annotate "$RESULT_DIR/callgrind.out" > "$RESULT_DIR/callgrind_report.txt"
echo -e "${GREEN}✓ 调用图分析完成${NC}"

# 显示热点函数
echo "  热点函数 Top 20:"
callgrind_annotate "$RESULT_DIR/callgrind.out" | head -n 40

echo -e "\n${YELLOW}[5/6] Massif - 堆内存使用分析${NC}"
echo "分析堆内存使用随时间的变化，需要 2-3 分钟..."
valgrind --tool=massif \
    --massif-out-file="$RESULT_DIR/massif.out" \
    ./bench_debug $SMALL_N 2>&1 | tee "$RESULT_DIR/massif.log"

# 生成报告
ms_print "$RESULT_DIR/massif.out" > "$RESULT_DIR/massif_report.txt"
echo -e "${GREEN}✓ 堆分析完成${NC}"

# 显示峰值内存
echo "  内存使用峰值:"
grep -A 5 "peak" "$RESULT_DIR/massif_report.txt" | head -n 10

echo -e "\n${YELLOW}[6/6] 生成综合报告${NC}"

# 创建 HTML 摘要报告
cat > "$RESULT_DIR/summary.html" << 'HTMLEOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Valgrind 性能分析报告</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; }
        h1 { color: #333; border-bottom: 3px solid #4CAF50; padding-bottom: 10px; }
        h2 { color: #4CAF50; margin-top: 30px; }
        .metric { background: #e8f5e9; padding: 15px; margin: 10px 0; border-radius: 5px; }
        .warning { background: #fff3e0; padding: 15px; margin: 10px 0; border-radius: 5px; }
        .error { background: #ffebee; padding: 15px; margin: 10px 0; border-radius: 5px; }
        .good { background: #e8f5e9; padding: 15px; margin: 10px 0; border-radius: 5px; }
        pre { background: #f5f5f5; padding: 10px; overflow-x: auto; }
        table { border-collapse: collapse; width: 100%; margin: 10px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #4CAF50; color: white; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔍 tcmalloc Valgrind 性能分析报告</h1>
        <p><strong>生成时间:</strong> $(date)</p>
        <p><strong>测试规模:</strong> $SMALL_N 次分配</p>
        
        <h2>📊 分析摘要</h2>
HTMLEOF

# 添加内存泄漏摘要
echo "<h3>1. 内存泄漏检测</h3>" >> "$RESULT_DIR/summary.html"
if grep -q "definitely lost: 0 bytes" "$RESULT_DIR/memcheck.txt"; then
    echo '<div class="good">✅ <strong>无内存泄漏</strong></div>' >> "$RESULT_DIR/summary.html"
else
    echo '<div class="error">⚠️ <strong>发现内存泄漏</strong></div>' >> "$RESULT_DIR/summary.html"
fi
echo "<pre>" >> "$RESULT_DIR/summary.html"
grep -A 10 "LEAK SUMMARY" "$RESULT_DIR/memcheck.txt" >> "$RESULT_DIR/summary.html" || echo "无数据" >> "$RESULT_DIR/summary.html"
echo "</pre>" >> "$RESULT_DIR/summary.html"

# 添加 Cache 性能
echo "<h3>2. Cache 性能</h3>" >> "$RESULT_DIR/summary.html"
echo "<pre>" >> "$RESULT_DIR/summary.html"
grep -E "I refs|I1 misses|D refs|D1 misses|LL misses" "$RESULT_DIR/cachegrind_report.txt" >> "$RESULT_DIR/summary.html" || echo "无数据" >> "$RESULT_DIR/summary.html"
echo "</pre>" >> "$RESULT_DIR/summary.html"

# 添加热点函数
echo "<h3>3. 性能热点函数 (Top 15)</h3>" >> "$RESULT_DIR/summary.html"
echo "<pre>" >> "$RESULT_DIR/summary.html"
callgrind_annotate "$RESULT_DIR/callgrind.out" | head -n 35 >> "$RESULT_DIR/summary.html"
echo "</pre>" >> "$RESULT_DIR/summary.html"

# 完成 HTML
cat >> "$RESULT_DIR/summary.html" << 'HTMLEOF'
        <h2>📁 详细报告文件</h2>
        <ul>
            <li><a href="memcheck.txt">memcheck.txt</a> - 内存错误和泄漏详细报告</li>
            <li><a href="cachegrind_report.txt">cachegrind_report.txt</a> - Cache 性能详细分析</li>
            <li><a href="callgrind_report.txt">callgrind_report.txt</a> - 函数调用和时间分析</li>
            <li><a href="massif_report.txt">massif_report.txt</a> - 堆内存使用分析</li>
        </ul>
        
        <h2>🛠️ 可视化工具</h2>
        <div class="metric">
            <p>使用 <code>kcachegrind</code> 查看调用图:</p>
            <pre>kcachegrind callgrind.out</pre>
        </div>
        
        <h2>💡 性能优化提示</h2>
        <ul>
            <li>关注 callgrind_report.txt 中占用 > 5% 时间的函数</li>
            <li>Cache miss rate 应该 < 5%</li>
            <li>检查是否有不必要的内存分配</li>
            <li>查看 massif 报告了解内存使用峰值</li>
        </ul>
    </div>
</body>
</html>
HTMLEOF

# 创建文本版摘要
cat > "$RESULT_DIR/README.txt" << 'EOF'
====================================
Valgrind 性能分析结果
====================================

分析完成时间: 
EOF
date >> "$RESULT_DIR/README.txt"

cat >> "$RESULT_DIR/README.txt" << 'EOF'

文件说明:
---------
1. memcheck.txt           - 内存泄漏和错误检测
2. cachegrind_report.txt  - Cache 性能分析
3. callgrind_report.txt   - 函数调用图和性能
4. massif_report.txt      - 堆内存使用分析
5. summary.html           - HTML 格式综合报告
6. *.out                  - 原始数据文件（供工具使用）

快速查看:
---------
# 内存泄漏
grep "LEAK SUMMARY" memcheck.txt

# 热点函数
head -n 40 callgrind_report.txt

# Cache 统计
grep -E "I refs|D refs|misses" cachegrind_report.txt

# 内存峰值
grep -A 5 "peak" massif_report.txt

可视化:
-------
# 安装 kcachegrind (如果未安装)
sudo apt-get install kcachegrind

# 查看调用图
kcachegrind callgrind.out

# 在浏览器中查看摘要
firefox summary.html  # 或其他浏览器
EOF

echo -e "${GREEN}✓ 报告生成完成${NC}"

# 最终摘要
echo -e "\n${BLUE}======================================"
echo -e "  分析完成！"
echo -e "======================================${NC}"
echo -e "结果目录: ${YELLOW}$RESULT_DIR${NC}"
echo ""
echo "生成的文件:"
ls -lh "$RESULT_DIR/"
echo ""
echo -e "${GREEN}快速查看命令:${NC}"
echo -e "  ${YELLOW}cd $RESULT_DIR${NC}"
echo -e "  ${YELLOW}cat README.txt${NC}              # 查看说明"
echo -e "  ${YELLOW}firefox summary.html${NC}        # 浏览器查看摘要"
echo -e "  ${YELLOW}less callgrind_report.txt${NC}  # 查看热点函数"
echo ""
echo -e "${GREEN}关键指标:${NC}"

# 显示关键指标
echo ""
echo "=== 内存泄漏 ==="
grep -A 5 "LEAK SUMMARY" "$RESULT_DIR/memcheck.txt" | head -n 10 || echo "无数据"

echo ""
echo "=== 热点函数 Top 10 ==="
callgrind_annotate "$RESULT_DIR/callgrind.out" 2>/dev/null | head -n 30 | tail -n 10 || echo "无数据"

echo ""
echo -e "${YELLOW}提示: 运行 'firefox $RESULT_DIR/summary.html' 查看详细的 HTML 报告${NC}"
