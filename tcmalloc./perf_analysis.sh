#!/bin/bash

# 性能分析脚本 - 使用 perf 和 Valgrind 分析 tcmalloc 项目
# 作者: Auto-generated
# 日期: 2025-11-12

set -e  # 遇到错误立即退出

echo "======================================"
echo "  tcmalloc 性能分析工具"
echo "======================================"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查工具是否安装
check_tool() {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}错误: $1 未安装${NC}"
        echo "请使用以下命令安装:"
        echo "  sudo apt-get install $2"
        return 1
    else
        echo -e "${GREEN}✓ $1 已安装${NC}"
        return 0
    fi
}

echo -e "\n${YELLOW}[1/5] 检查依赖工具...${NC}"
TOOLS_OK=true
check_tool "perf" "linux-tools-common linux-tools-generic" || TOOLS_OK=false
check_tool "valgrind" "valgrind" || TOOLS_OK=false

if [ "$TOOLS_OK" = false ]; then
    echo -e "${YELLOW}提示: 可以选择性地只使用已安装的工具${NC}"
fi

# 编译调试版本
echo -e "\n${YELLOW}[2/5] 编译带符号的调试版本...${NC}"
make bench_debug
echo -e "${GREEN}✓ 编译完成${NC}"

# 创建结果目录
RESULT_DIR="perf_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULT_DIR"
echo -e "${GREEN}✓ 结果将保存到: $RESULT_DIR${NC}"

# ============ Perf 分析 ============
if command -v perf &> /dev/null; then
    echo -e "\n${YELLOW}[3/5] 使用 perf 进行性能分析...${NC}"
    
    # 3.1 CPU 性能分析
    echo -e "\n  ${YELLOW}3.1 CPU 性能热点分析 (perf record)...${NC}"
    perf record -F 99 -g --call-graph dwarf -o "$RESULT_DIR/perf.data" ./bench_debug 100000
    echo -e "  ${GREEN}✓ 数据已收集${NC}"
    
    # 生成报告
    echo -e "  ${YELLOW}生成性能报告...${NC}"
    perf report -i "$RESULT_DIR/perf.data" --stdio > "$RESULT_DIR/perf_report.txt" 2>&1
    perf report -i "$RESULT_DIR/perf.data" --stdio --sort comm,dso,symbol > "$RESULT_DIR/perf_hotspot.txt" 2>&1
    echo -e "  ${GREEN}✓ 报告已生成: perf_report.txt, perf_hotspot.txt${NC}"
    
    # 3.2 CPU 统计
    echo -e "\n  ${YELLOW}3.2 CPU 事件统计 (perf stat)...${NC}"
    perf stat -e cycles,instructions,cache-misses,cache-references,branches,branch-misses \
        -o "$RESULT_DIR/perf_stat.txt" ./bench_debug 100000
    echo -e "  ${GREEN}✓ 统计已保存: perf_stat.txt${NC}"
    
    # 3.3 Cache 性能分析
    echo -e "\n  ${YELLOW}3.3 Cache 性能分析...${NC}"
    perf stat -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses \
        -o "$RESULT_DIR/perf_cache.txt" ./bench_debug 100000 2>&1
    echo -e "  ${GREEN}✓ Cache 分析已保存: perf_cache.txt${NC}"
    
else
    echo -e "\n${RED}[3/5] 跳过 perf 分析 (未安装)${NC}"
fi

# ============ Valgrind 分析 ============
if command -v valgrind &> /dev/null; then
    echo -e "\n${YELLOW}[4/5] 使用 Valgrind 进行内存分析...${NC}"
    
    # 4.1 Memcheck - 内存错误检测
    echo -e "\n  ${YELLOW}4.1 内存错误检测 (Memcheck)...${NC}"
    valgrind --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --verbose \
        --log-file="$RESULT_DIR/valgrind_memcheck.txt" \
        ./bench_debug 10000
    echo -e "  ${GREEN}✓ 内存检测完成: valgrind_memcheck.txt${NC}"
    
    # 4.2 Cachegrind - Cache 性能分析
    echo -e "\n  ${YELLOW}4.2 Cache 性能分析 (Cachegrind)...${NC}"
    valgrind --tool=cachegrind \
        --cachegrind-out-file="$RESULT_DIR/cachegrind.out" \
        ./bench_debug 10000
    cg_annotate "$RESULT_DIR/cachegrind.out" > "$RESULT_DIR/cachegrind_report.txt"
    echo -e "  ${GREEN}✓ Cache 分析完成: cachegrind_report.txt${NC}"
    
    # 4.3 Callgrind - 调用图分析
    echo -e "\n  ${YELLOW}4.3 调用图分析 (Callgrind)...${NC}"
    valgrind --tool=callgrind \
        --callgrind-out-file="$RESULT_DIR/callgrind.out" \
        ./bench_debug 10000
    callgrind_annotate "$RESULT_DIR/callgrind.out" > "$RESULT_DIR/callgrind_report.txt"
    echo -e "  ${GREEN}✓ 调用图分析完成: callgrind_report.txt${NC}"
    echo -e "  ${YELLOW}提示: 使用 kcachegrind 可视化查看: kcachegrind $RESULT_DIR/callgrind.out${NC}"
    
    # 4.4 Massif - 堆内存分析
    echo -e "\n  ${YELLOW}4.4 堆内存使用分析 (Massif)...${NC}"
    valgrind --tool=massif \
        --massif-out-file="$RESULT_DIR/massif.out" \
        ./bench_debug 10000
    ms_print "$RESULT_DIR/massif.out" > "$RESULT_DIR/massif_report.txt"
    echo -e "  ${GREEN}✓ 堆分析完成: massif_report.txt${NC}"
    
else
    echo -e "\n${RED}[4/5] 跳过 Valgrind 分析 (未安装)${NC}"
fi

# 生成总结报告
echo -e "\n${YELLOW}[5/5] 生成分析总结...${NC}"
cat > "$RESULT_DIR/README.md" << 'EOF'
# tcmalloc 性能分析报告

## 分析工具和文件说明

### Perf 性能分析
- **perf_report.txt**: CPU 性能热点分析，显示哪些函数占用 CPU 时间最多
- **perf_hotspot.txt**: 按符号排序的性能热点
- **perf_stat.txt**: CPU 事件统计（周期、指令、分支预测等）
- **perf_cache.txt**: Cache 性能统计（L1/L3 缓存命中率）

### Valgrind 内存分析
- **valgrind_memcheck.txt**: 内存泄漏和错误检测
- **cachegrind_report.txt**: 缓存模拟分析报告
- **callgrind_report.txt**: 函数调用图和性能分析
- **massif_report.txt**: 堆内存使用随时间变化

## 如何阅读结果

### 1. 查看 CPU 热点
```bash
less perf_report.txt
# 查找 Overhead 百分比最高的函数
```

### 2. 查看 Cache 性能
```bash
grep -A 10 "cache-misses" perf_stat.txt
# cache-miss 率越低越好，一般应低于 5%
```

### 3. 检查内存泄漏
```bash
grep "LEAK SUMMARY" valgrind_memcheck.txt
# 应该显示 "no leaks are possible"
```

### 4. 可视化分析（需要 kcachegrind）
```bash
kcachegrind callgrind.out
```

## 性能优化建议

根据分析结果，重点关注：
1. **CPU 热点**: perf_report.txt 中 Overhead > 5% 的函数
2. **Cache 命中率**: 应该 > 95%
3. **内存泄漏**: 应该为 0
4. **锁竞争**: 在多线程测试中的同步开销

## 下一步

1. 对比不同内存分配大小的性能
2. 对比不同线程数的性能
3. 与系统 malloc 进行对比测试
EOF

echo -e "${GREEN}✓ 总结报告已生成: $RESULT_DIR/README.md${NC}"

# 显示摘要
echo -e "\n${GREEN}======================================"
echo -e "  分析完成！"
echo -e "======================================${NC}"
echo -e "结果目录: ${YELLOW}$RESULT_DIR${NC}"
echo ""
echo "快速查看命令:"
echo -e "  ${YELLOW}cd $RESULT_DIR${NC}"
echo -e "  ${YELLOW}cat README.md${NC}          # 查看说明"
echo -e "  ${YELLOW}less perf_report.txt${NC}   # 查看 CPU 热点"
echo -e "  ${YELLOW}less valgrind_memcheck.txt${NC}  # 查看内存检测"
echo ""
echo "生成的文件列表:"
ls -lh "$RESULT_DIR/"

echo -e "\n${GREEN}提示: 建议先查看 README.md 了解各个报告的含义${NC}"
