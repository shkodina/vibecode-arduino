# encoding: UTF-8
# SketchUp Pro 2022: импортирует три DAE как отдельные объекты.
#
#   Window -> Ruby Console
#   load 'ПУТЬ/korpus/korpus_fonarya.rb'
#
# Геометрия лежит в dae/*.dae (скруглённый корпус). Этот скрипт только
# подтягивает их в модель.

mod = Sketchup.active_model
papka = File.dirname(__FILE__)
chasti = %w[korpus vyemnyy_blok SF_12045_ne_pechatat]

mod.start_operation("Import korpusa fonarya", true)
begin
  chasti.each do |imya|
    put = File.join(papka, "dae", "#{imya}.dae")
    unless File.exist?(put)
      puts "NET FAILA: #{put}"
      next
    end
    puts "Import #{imya}..."
    ok = mod.import(put)
    puts(ok ? "  ok" : "  ne vyshlo, File>Import vruchnuyu: #{put}")
  end
  mod.active_view.zoom_extents
  mod.commit_operation
  puts "Gotovo. Save As SKP. Pechatat stl/korpus.stl i stl/vyemnyy_blok.stl"
rescue => e
  mod.abort_operation
  puts "OSHIBKA: #{e.message}"
end
