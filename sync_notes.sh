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
TIMESTEMP=$(date '+%Y-%m-%d %H:%M:%S')

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

# 按第一级目录进行归类
declare -A kinds
declare -a root_files=() # 数组 根目录下的文件单独收集

for file in "${changed_files[@]}"; do
    # 跳过空行
    [ -z "$file" ] && continue 

    dir="${file%%/*}"

    if [ "$dir" = "$file" ]; then
        root_files+=("$file")
    else
        # 保留第一级目录
        kind="$dir"
        kinds["$kind"]=1
    fi
done

# 根目录下的文件
if [ ${#root_files[@]} -gt 0 ]; then
    echo "process the files in root: ${root_files[*]}"
    for f in "${root_files[@]}"; do
        git add "$f"
    done
    if ! git diff --cached --quiet; then
        git commit -m "[$HOSTNAME] [$TIMESTEMP] | root"
    fi
fi

# 逐类提交
for kind in "${!kinds[@]}"; do
    echo "processing kind: $kind"
    git add "$kind"/
    if git diff --cached --quiet; then
        echo "  $kind has no cached change continue."
        continue 
    fi
    git commit -m "[$HOSTNAME] [$TIMESTEMP] | $kind"
done

# 提交
git push

printf "\n\n\n"

echo "All Note Commits Are Already Pushed."

printf "\n\n\n"

git status

