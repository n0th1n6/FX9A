#!/usr/bin/env python3
"""Link production settings, state, VFO and CAT modules with mocked EEPROM/RF."""
from pathlib import Path
import os, re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
native = (root/'cmsis_ll').exists()
bsp = root/'RadioStation/BSP'
app = root/'cmsis_ll/app' if native else bsp
headers = root/'cmsis_ll/board' if native else bsp
with tempfile.TemporaryDirectory(prefix='fx9a-settings-cat-') as directory:
    temp = Path(directory)
    for folder in (bsp, headers):
        for path in folder.glob('*.h'):
            text = path.read_text(encoding='latin1')
            if path.name == 'bsp_board.h':
                text = text.replace('#include "stm32f10x_conf.h"','').replace('#include "bsp_delay.h"','').replace('#include "stm32f1xx.h"','')
            (temp/path.name).write_text(text)
    (temp/'cmsis_gcc.h').write_text('#include <stdint.h>\nextern uint32_t settings_irq_mask;\nstatic inline uint32_t __get_PRIMASK(void) { return settings_irq_mask; }\nstatic inline void __disable_irq(void) { settings_irq_mask = 1; }\nstatic inline void __set_PRIMASK(uint32_t mask) { settings_irq_mask = mask; }\n')
    sources = []
    for folder, name in ((bsp,'settings_store.c'),(bsp,'bsp_kenwood.c'),(app,'radio_state.c'),
                         (app,'frequency_state.c' if native else 'radio_vfo.c')):
        p=temp/name; p.write_text((folder/name).read_text(encoding='latin1')); sources.append(str(p))
    synth = app/'radio_synth.c' if native else app/'radio_rf.c'
    text = synth.read_text(encoding='latin1')
    pieces=[]
    for name in ('RX_AlignmentOffset','Get_DDSLO','Get_DDSBFO'):
        m=re.search(r'(?:static )?(?:int16_t|uint32_t) '+name+r'\([^)]*\)\s*\{',text)
        end, depth=m.end(), 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}'); end += 1
        pieces.append(text[m.start():end])
    p=temp/'synth.c'; p.write_text('#include "bsp_board.h"\n#include "radio_state.h"\n'+'\n'.join(pieces)); sources.append(str(p))
    program=(root/'tools/feature_tests/settings_cat_main.c').read_text()
    controls=(app/'radio_controls.c').read_text(encoding='latin1')
    start=controls.index('    case 1:'); end=controls.index('    case 2:',start)
    branch=controls[start:end]
    program=program.replace('int main(void) {',
        'static void power_button(void) { KEY_AttrDef key = {.val=1,.state=keyLong}; switch(key.val) {\n'+branch+'} }\nint main(void) {')
    commands = sorted(set(a+b for a,b in re.findall(r"pBuf\[0\] == '(.)' && pBuf\[1\] == '(.)'", (bsp/'bsp_kenwood.c').read_text(encoding='latin1'))))
    queries = [dict(MR='MR01', SM='SM0', TX='TX0').get(cmd,cmd) for cmd in commands if cmd not in ('RX','RC','RD','RU')]
    program=re.sub(r'const char \*queries\[\] = \{.*?\};', 'const char *queries[] = {' + ','.join('"'+q+'"' for q in queries) + '};', program)
    program=program.replace('RESET()', 'ResetToDefault()' if native else 'RadioState_ResetDefaults()')
    program=program.replace('LOAD()', 'software_init()' if native else 'RadioState_Load()')
    (temp/'main.c').write_text(program)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
        '-Wno-pointer-sign','-fshort-enums','-fsanitize=undefined,address','-I',str(temp),
        *sources,str(temp/'main.c'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
