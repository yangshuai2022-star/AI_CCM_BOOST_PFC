function build_mex()
root=fileparts(fileparts(mfilename('fullpath'))); out=fullfile(root,'build');
if ~isfolder(out), mkdir(out); end
clear dsp_sfun
if ispc && isfolder('D:\MULT_PFC_DEFINE\toolchains\mingw64')
    setenv('MW_MINGW64_LOC','D:\MULT_PFC_DEFINE\toolchains\mingw64');
    mex('-setup','C');
end
mex('-R2017b','CFLAGS=$CFLAGS -std=c99 -Wall -Wextra',...
    ['-I' fullfile(matlabroot,'simulink','include')],['-I' fullfile(root,'include')],...
    '-outdir',out,fullfile(root,'matlab','dsp_sfun.c'),fullfile(root,'src','dsp_behavior.c'),...
    fullfile(root,'src','pfc_control.c'));
addpath(out);
end
