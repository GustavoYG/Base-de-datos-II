import markdown
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

with open(r'REPORTE_FINAL.md', 'r', encoding='utf-8') as f:
    md_content = f.read()

html_content = markdown.markdown(md_content, extensions=['tables', 'fenced_code'])

html_full = (
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '    <meta charset="utf-8">\n'
    '    <title>Reporte Final - Base de Datos II</title>\n'
    '    <style>\n'
    '        body { font-family: Arial, sans-serif; margin: 40px; line-height: 1.6; }\n'
    '        h1 { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; }\n'
    '        h2 { color: #34495e; margin-top: 30px; }\n'
    '        h3 { color: #7f8c8d; }\n'
    '        table { border-collapse: collapse; width: 100%; margin: 20px 0; }\n'
    '        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n'
    '        th { background-color: #3498db; color: white; }\n'
    '        tr:nth-child(even) { background-color: #f2f2f2; }\n'
    '        code { background-color: #f4f4f4; padding: 2px 5px; border-radius: 3px; }\n'
    '        pre { background-color: #2d2d2d; color: #f8f8f2; padding: 15px; border-radius: 5px; overflow-x: auto; }\n'
    '        pre code { background-color: transparent; color: inherit; }\n'
    '        @media print { body { margin: 20px; } }\n'
    '    </style>\n'
    '</head>\n'
    '<body>\n'
    + html_content +
    '\n</body>\n</html>'
)

with open(r'REPORTE_FINAL.html', 'w', encoding='utf-8') as f:
    f.write(html_full)

print('HTML generado: REPORTE_FINAL.html')
print('Abre el archivo en tu navegador y usa Ctrl+P para imprimir como PDF')
