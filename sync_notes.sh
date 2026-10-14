#!/usr/bin/env bash
set -euo pipefail

# 指定目录，默认为脚本启动时所在的目录
CONFIG_DIR="${1:-.}"
cd "$CONFIG_DIR"

# 检查是否为 git 仓库
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "error: current directory is not git repo." >&2
    exit 1
fi

# 收集 主机名 时间戳
HOSTNAME=$(hostname)
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

# 获取所有未提交的更改
mapfile -d '' -t changed_files < <(
    git diff HEAD --name-only -z
    git ls-files --others --exclude-standard -z
)

if [ ${#changed_files[@]} -eq 0 ]; then
    echo "has no file need to commit."
    printf "\n"
    git status
    exit 0
fi

# 按前两级目录进行归类：kind -> 该分类下的文件（换行分隔）
declare -A kinds
declare -a root_files=() # 根目录下的文件单独收集

for file in "${changed_files[@]}"; do
    # 跳过空行
    [ -z "$file" ] && continue

    IFS='/' read -ra parts <<< "$file"
    n=${#parts[@]}

    if [ "$n" -eq 1 ]; then
        # 根目录下的文件
        root_files+=("$file")
        continue
    fi

    if [ "$n" -eq 2 ]; then
        # 位于第一级目录下的文件，只取一级
        kind="${parts[0]}"
    else
        # 至少两级目录，取前两级
        kind="${parts[0]}/${parts[1]}"
    fi

    kinds["$kind"]+="$file"$'\n'
done

# 根目录下的文件
if [ ${#root_files[@]} -gt 0 ]; then
    echo "process the files in root: ${root_files[*]}"
    for f in "${root_files[@]}"; do
        git add -- "$f"
    done
    if ! git diff --cached --quiet; then
        git commit -m "[$HOSTNAME] [$TIMESTAMP] | root"
    fi
fi

# 逐类提交
for kind in "${!kinds[@]}"; do
    echo "processing kind: $kind"
    # 逐个文件加入，避免不同 kind 之间的交叉污染
    while IFS= read -r f; do
        [ -z "$f" ] && continue
        git add -- "$f"
    done <<< "${kinds[$kind]}"

    if git diff --cached --quiet; then
        echo "  $kind has no cached change continue."
        continue
    fi
    git commit -m "[$HOSTNAME] [$TIMESTAMP] | $kind"
done

# 提交
git push

printf "\n\n\n"

echo "All Note Commits Are Already Pushed."

printf "\n\n\n"

