#!/usr/bin/env python
#coding=utf-8

import os
#import glob
path1=os.getcwd()
def filerename(path):
	filenames = os.listdir(path)#将path路径下的所有文件名存入列表filenames 
	for filename in filenames:#循环遍历每个文件
		if (filename=='rename.py'):
			continue
		domain = os.path.abspath(path)#获取规范的路径
		filename = os.path.join(domain,filename)#带路径的文件名
		if os.path.isdir(filename): # 如果是文件夹进入递归
			filerename(filename)
			continue
		print (filename)
		fread = open(filename,'r+')
		alllines=fread.readlines()
		fread.seek(0)
		fread.truncate()
		for eachline in alllines:
			eachline = eachline.replace('S5K3L6', 'S5K3L6_MAIN')  # imx258 替换为  imx214
			eachline = eachline.replace('s5k3l6', 's5k3l6_main')  # imx258 替换为  imx214
			fread.write(eachline)
		fread.close()
		
def rename(path):
	filelist=os.listdir(path)
	#fileAddr = glob.glob(path)	
	for files in filelist:	#遍历所有文件
		Olddir=os.path.join(path,files)	#原来的文件路径
		if os.path.isdir(Olddir):	#如果是文件夹进入递归
			rename(Olddir);
		filename=os.path.splitext(files)[0]	#文件名
		filetype=os.path.splitext(files)[1]	#文件扩展名
		if filename.find('s5k3l6')>=0:#如果文件名中含有
			Newdir=os.path.join(path,filename.split('s5k3l6')[0]+'s5k3l6_main'+filename.split('s5k3l6')[1]+filetype) 	#取---前面的字符，若需要取后面的字符则使用filename.split('---')[1]
			if not os.path.isfile(Newdir):
				os.rename(Olddir,Newdir)		
		#print(fileAddr)
	
rename(path1)
filerename(path1)