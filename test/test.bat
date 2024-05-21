..\build\Release\avm null.json
fc res.json null.json > test.txt
..\build\Release\avm true.json
fc res.json true.json >> test.txt 
..\build\Release\avm false.json
fc res.json false.json >> test.txt
..\build\Release\avm [null].json
fc res.json [null].json >> test.txt 
..\build\Release\avm [true].json
fc res.json [true].json >> test.txt
..\build\Release\avm [false].json
fc res.json [false].json >> test.txt
..\build\Release\avm array.json
fc res.json array.json>> test.txt
..\build\Release\avm nulls.json
fc res.json nulls.json  >> test.txt
..\build\Release\avm number.json
fc res.json number.json  >> test.txt
..\build\Release\avm strings.json 
fc res.json strings.json >> test.txt
..\build\Release\avm text.json 
fc res.json text.json >> test.txt
..\build\Release\avm text2.json 
fc res.json text2.json >> test.txt