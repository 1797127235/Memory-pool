#!/bin/bash

# 快速性能分析 - 只做最关键的几项检查
# 适合日常快速测试使用

echo "======================================"
echo "  快速性能分析"
echo "======================================"

# 编译
echo "[1] 编译调试版本..."
make bench_debug

# Perf 热点分析
if command -v perf &> /dev/null; then
    echo -e "\n[2] CPU 热点分析 (30秒)..."
    echo "运行程序并收集数据..."
    perf record -F 99 -g ./bench_debug 100000
    
    echo -e "\n--- Top 20 热点函数 ---"
    perf report --stdio -n --sort overhead,symbol | head -n 40
    
    echo -e "\n[3] CPU 性能统计..."
    perf stat ./bench_debug 100000
else
    echo "[2-3] 跳过 (perf 未安装)"
fi

# Valgrind 快速内存检查
if command -v valgrind &> /dev/null; then
    echo -e "\n[4] 内存泄漏检查 (减少迭代次数加速)..."
    valgrind --leak-check=full --show-leak-kinds=all ./bench_debug 10000 2>&1 | grep -A 20 "LEAK SUMMARY"
else
    echo "[4] 跳过 (valgrind 未安装)"
fi

echo -e "\n======================================"
echo "  快速分析完成"
echo "======================================"
echo "如需详细分析，请运行: ./perf_analysis.sh"
