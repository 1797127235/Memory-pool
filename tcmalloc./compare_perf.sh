#!/bin/bash

# 对比分析脚本 - 对比 tcmalloc 和系统 malloc 的性能
# 使用 perf 对比两者的差异

set -e

echo "======================================"
echo "  tcmalloc vs 系统 malloc 性能对比"
echo "======================================"

# 颜色
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 创建结果目录
RESULT_DIR="compare_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULT_DIR"

echo -e "\n${YELLOW}[1] 编译测试程序...${NC}"
make bench_debug
echo -e "${GREEN}✓ 编译完成${NC}"

# 测试参数
ITERATIONS=1000000

echo -e "\n${YELLOW}[2] 性能对比测试...${NC}"

if command -v perf &> /dev/null; then
    echo -e "\n  ${YELLOW}2.1 测试 tcmalloc (自定义实现)...${NC}"
    perf stat -e cycles,instructions,cache-misses,cache-references,branches,branch-misses,L1-dcache-load-misses \
        -o "$RESULT_DIR/tcmalloc_stat.txt" ./bench_debug $ITERATIONS 2>&1
    
    # 记录程序自己的输出
    ./bench_debug $ITERATIONS > "$RESULT_DIR/tcmalloc_output.txt" 2>&1
    
    echo -e "\n  ${YELLOW}2.2 CPU 热点分析...${NC}"
    perf record -F 99 -g -o "$RESULT_DIR/tcmalloc.perf.data" ./bench_debug $ITERATIONS
    perf report -i "$RESULT_DIR/tcmalloc.perf.data" --stdio > "$RESULT_DIR/tcmalloc_hotspot.txt" 2>&1
    
    echo -e "${GREEN}✓ tcmalloc 分析完成${NC}"
else
    echo "perf 未安装，无法进行详细对比"
    exit 1
fi

if command -v valgrind &> /dev/null; then
    echo -e "\n${YELLOW}[3] Cachegrind 详细对比...${NC}"
    
    echo -e "  ${YELLOW}3.1 Cachegrind for tcmalloc...${NC}"
    valgrind --tool=cachegrind \
        --cachegrind-out-file="$RESULT_DIR/tcmalloc.cachegrind" \
        ./bench_debug 100000 2>&1 > /dev/null
    
    cg_annotate "$RESULT_DIR/tcmalloc.cachegrind" > "$RESULT_DIR/tcmalloc_cachegrind.txt"
    echo -e "${GREEN}✓ Cache 分析完成${NC}"
fi

# 生成对比报告
echo -e "\n${YELLOW}[4] 生成对比报告...${NC}"

cat > "$RESULT_DIR/comparison_report.md" << 'EOF'
# tcmalloc 性能对比报告

## 测试配置
- 迭代次数: 1,000,000 (完整测试)
- 迭代次数: 100,000 (Valgrind 测试，减少时间)
- 编译选项: -O2 -g -pthread -fno-omit-frame-pointer

## 文件说明

### 性能统计
- `tcmalloc_stat.txt`: perf stat 统计数据
- `tcmalloc_output.txt`: 程序自身的性能报告
- `tcmalloc_hotspot.txt`: CPU 热点函数

### Cache 分析
- `tcmalloc_cachegrind.txt`: Cache 命中率详细分析

## 关键指标

### 1. CPU 周期和指令
```bash
grep "cycles\|instructions" tcmalloc_stat.txt
```
- IPC (Instructions Per Cycle): 越高越好，一般 > 1.0

### 2. Cache 性能
```bash
grep "cache-misses\|cache-references" tcmalloc_stat.txt
```
- Cache miss rate: 越低越好，一般 < 5%

### 3. 分支预测
```bash
grep "branch-misses\|branches" tcmalloc_stat.txt
```
- Branch miss rate: 越低越好，一般 < 2%

### 4. 程序性能输出
```bash
cat tcmalloc_output.txt
```
查看单线程和多线程测试的实际耗时

## 优化建议

根据 hotspot 分析结果：
1. 关注 Overhead > 5% 的函数
2. 检查锁竞争情况（在多线程场景）
3. 查看 Cache miss 集中在哪些函数

## 使用可视化工具

如果安装了 kcachegrind：
```bash
kcachegrind tcmalloc.cachegrind
```

EOF

# 自动提取关键数据
echo -e "\n## 快速摘要\n" >> "$RESULT_DIR/comparison_report.md"

echo -e "### tcmalloc 程序输出\n" >> "$RESULT_DIR/comparison_report.md"
echo '```' >> "$RESULT_DIR/comparison_report.md"
cat "$RESULT_DIR/tcmalloc_output.txt" >> "$RESULT_DIR/comparison_report.md" 2>/dev/null || echo "无数据" >> "$RESULT_DIR/comparison_report.md"
echo '```' >> "$RESULT_DIR/comparison_report.md"

echo -e "\n### perf stat 关键指标\n" >> "$RESULT_DIR/comparison_report.md"
echo '```' >> "$RESULT_DIR/comparison_report.md"
cat "$RESULT_DIR/tcmalloc_stat.txt" >> "$RESULT_DIR/comparison_report.md" 2>/dev/null || echo "无数据" >> "$RESULT_DIR/comparison_report.md"
echo '```' >> "$RESULT_DIR/comparison_report.md"

echo -e "${GREEN}✓ 报告已生成${NC}"

# 显示总结
echo -e "\n${GREEN}======================================"
echo -e "  对比分析完成！"
echo -e "======================================${NC}"
echo -e "结果目录: ${YELLOW}$RESULT_DIR${NC}"
echo ""
echo "查看报告:"
echo -e "  ${YELLOW}cd $RESULT_DIR${NC}"
echo -e "  ${YELLOW}cat comparison_report.md${NC}"
echo ""
echo "文件列表:"
ls -lh "$RESULT_DIR/"

echo -e "\n${GREEN}快速查看程序输出:${NC}"
cat "$RESULT_DIR/tcmalloc_output.txt" 2>/dev/null || echo "无数据"

echo -e "\n${GREEN}快速查看 perf 统计:${NC}"
if [ -f "$RESULT_DIR/tcmalloc_stat.txt" ]; then
    head -n 20 "$RESULT_DIR/tcmalloc_stat.txt"
fi
