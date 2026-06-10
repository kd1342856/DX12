import json

files_data = {}

with open(r'C:\Users\taiza\.gemini\antigravity\brain\593805c4-cbba-4786-9fc6-cf3479d5a2e1\.system_generated\logs\transcript.jsonl', 'r', encoding='utf-8') as f:
    for line in f:
        try:
            obj = json.loads(line)
            if 'tool_calls' in obj:
                for call in obj['tool_calls']:
                    name = call['name']
                    args = call.get('args', {})
                    if name == 'write_to_file':
                        target = args.get('TargetFile', '').strip('"')
                        content = args.get('CodeContent', '')
                        files_data[target] = content
                    elif name == 'replace_file_content':
                        target = args.get('TargetFile', '').strip('"')
                        target_str = args.get('TargetContent', '')
                        rep_str = args.get('ReplacementContent', '')
                        if target in files_data:
                            files_data[target] = files_data[target].replace(target_str, rep_str)
        except Exception as e:
            pass

for target, content in files_data.items():
    if target and target.endswith('.cpp') or target.endswith('.h'):
        print(f'Recovered: {target}')
        with open(target + '.recovered', 'w', encoding='utf-8') as out:
            out.write(content)
