# mktorrent.py OUT.torrent PATH [name]: a v1 torrent of a file or folder, no tracker.
import hashlib, os, sys
def be(x):
    if isinstance(x,int): return b'i%de'%x
    if isinstance(x,str): x=x.encode()
    if isinstance(x,bytes): return b'%d:'%len(x)+x
    if isinstance(x,list): return b'l'+b''.join(be(i) for i in x)+b'e'
    if isinstance(x,dict): return b'd'+b''.join(be(k)+be(x[k]) for k in sorted(x))+b'e'
out,path=sys.argv[1],sys.argv[2]
plen=256*1024
files=[]
if os.path.isdir(path):
    for root,_,fs in os.walk(path):
        for f in sorted(fs):
            p=os.path.join(root,f); files.append((os.path.relpath(p,path).split(os.sep),p))
    files.sort()
else: files=[([os.path.basename(path)],path)]
pieces=b''; buf=b''
for _,p in files:
    with open(p,'rb') as fh:
        while True:
            c=fh.read(plen-len(buf))
            if not c: break
            buf+=c
            if len(buf)==plen: pieces+=hashlib.sha1(buf).digest(); buf=b''
if buf: pieces+=hashlib.sha1(buf).digest()
info={'name':os.path.basename(path.rstrip('/')),'piece length':plen,'pieces':pieces}
if os.path.isdir(path): info['files']=[{'length':os.path.getsize(p),'path':parts} for parts,p in files]
else: info['length']=os.path.getsize(path)
open(out,'wb').write(be({'info':info,'created by':'uqbit tests'}))
print(hashlib.sha1(be(info)).hexdigest())
