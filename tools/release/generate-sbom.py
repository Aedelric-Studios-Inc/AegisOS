#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json,os,pathlib,subprocess,datetime
SKIP={'.git','build','target','release','node_modules','__pycache__'}
def digest(p):
 h=hashlib.sha256();
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--root',default='.');ap.add_argument('--output',default='build/pr1/AegisOS-2.0.0-pre.1.spdx.json');a=ap.parse_args()
 root=pathlib.Path(a.root).resolve(); files=[]
 for p in sorted(root.rglob('*')):
  if not p.is_file() or any(x in SKIP for x in p.relative_to(root).parts):continue
  rel=p.relative_to(root).as_posix();files.append({'SPDXID':'SPDXRef-File-'+hashlib.sha1(rel.encode()).hexdigest(),'fileName':'./'+rel,'checksums':[{'algorithm':'SHA256','checksumValue':digest(p)}]})
 try: rev=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,stderr=subprocess.DEVNULL).strip()
 except Exception: rev='unknown'
 epoch=int(os.environ.get('SOURCE_DATE_EPOCH','0') or 0);created=datetime.datetime.fromtimestamp(epoch,datetime.timezone.utc).isoformat().replace('+00:00','Z')
 doc={'spdxVersion':'SPDX-2.3','dataLicense':'CC0-1.0','SPDXID':'SPDXRef-DOCUMENT','name':'AegisOS-2.0.0-pre.1','documentNamespace':f'https://aedelricstudiosinc.com/sbom/aegisos/{rev}','creationInfo':{'created':created,'creators':['Organization: Aedelric Studios Workspace','Tool: AegisOS generate-sbom.py']},'packages':[{'name':'AegisOS','SPDXID':'SPDXRef-Package-AegisOS','versionInfo':'2.0.0-pre.1','downloadLocation':'NOASSERTION','filesAnalyzed':True,'licenseConcluded':'LicenseRef-Proprietary','licenseDeclared':'LicenseRef-Proprietary','copyrightText':'NOASSERTION'}],'files':files,'relationships':[{'spdxElementId':'SPDXRef-Package-AegisOS','relationshipType':'CONTAINS','relatedSpdxElement':x['SPDXID']} for x in files]}
 out=pathlib.Path(a.output);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(doc,indent=2,sort_keys=True)+'\n');print(out)
if __name__=='__main__':main()
