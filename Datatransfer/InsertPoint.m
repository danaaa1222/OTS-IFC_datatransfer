function [Waveform_back] = InsertPoint( Waveform,insertpoint )

x = 1:1:length(Waveform); 
% insertpoint=0;
v =double( Waveform);
 xq=1:(1/(insertpoint+1)):length(Waveform);
Waveform_back= interp1(x,v,xq);
end