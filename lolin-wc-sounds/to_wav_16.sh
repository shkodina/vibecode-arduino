#!/bin/bash
set -euo pipefail

if [[ $# -lt 1 || -z "${1:-}" ]]; then
  echo "Ошибка: укажите целевую папку первым параметром." >&2
  echo "Использование: $0 <целевая_папка>" >&2
  exit 1
fi

src_dir=$(realpath -m "$1")
src_dir="${src_dir%/}"

if [[ ! -d "$src_dir" ]]; then
  echo "Ошибка: папка не найдена: $1" >&2
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "Ошибка: ffmpeg не найден в PATH." >&2
  exit 1
fi

base_name=$(basename "$src_dir")
parent_dir=$(dirname "$src_dir")
out_dir="${parent_dir}/${base_name}_wav_16"
result_log="$out_dir/conv_result.txt"

mkdir -p "$out_dir"
: > "$result_log"

list_file=$(mktemp /tmp/to_wav_16.XXXXXX)
trap 'rm -f "$list_file"' EXIT

find "$src_dir" -type f \( -iname '*.mp3' -o -iname '*.wav' \) | sort > "$list_file"

echo "Источник: $src_dir"
echo "Назначение: $out_dir"
echo "Список файлов: $list_file ($(wc -l < "$list_file") шт.)"

converted=0
failed=0
declare -A dir_idx

while IFS= read -r src_file; do
  [[ -z "$src_file" ]] && continue

  rel_path="${src_file#"$src_dir"/}"
  rel_dir=$(dirname "$rel_path")

  if [[ "$rel_dir" == "." ]]; then
    dest_dir="$out_dir"
    out_rel_dir="."
  else
    dest_dir="$out_dir/$rel_dir"
    out_rel_dir="$rel_dir"
  fi

  idx=${dir_idx[$rel_dir]:-0}
  num=$(printf '%02d' "$idx")
  dir_idx[$rel_dir]=$((idx + 1))

  dest_file="$dest_dir/${num}.wav"
  if [[ "$out_rel_dir" == "." ]]; then
    dest_rel="${num}.wav"
  else
    dest_rel="${out_rel_dir}/${num}.wav"
  fi

  mkdir -p "$dest_dir"

  echo "→ $rel_path -> $dest_rel"
  if ffmpeg -nostdin -y -hide_banner -loglevel error \
      -i "$src_file" \
      -ar 16000 -ac 1 -c:a pcm_s16le \
      "$dest_file"; then
    echo "$rel_path -> $dest_rel" >> "$result_log"
    converted=$((converted + 1))
  else
    echo "  ошибка конвертации: $src_file" >&2
    failed=$((failed + 1))
  fi
done < <(cat "$list_file")

echo "Готово: успешно $converted, ошибок $failed"
echo "Карта конвертации: $result_log"
if [[ "$failed" -gt 0 ]]; then
  exit 1
fi
