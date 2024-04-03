%----------------setting configure----------------------------------------------
PulPol = 1;             %"1" means positive pulses/"-1"means negtive pulses
plot_moving_avg=0;    %1=on 0=off
move_background=1;    %1=on 0=off
      
Period =32*3;              %在窗口包含整个脉冲时，需要手动输入周期-
                          % 一定要和脉冲采集的点数相等。Number of samples per period （change the Period!!!!!!!!）
%-----------------------------------------------------------------------------------

%----------------设置恢复数据文件路径---------------------------------------------
 pathway='sample\';
%-------------------------------------------------------------------------------

%------------------设置保存的图片位置路径 格式 后缀------------------------------
    path = 'Img\';  %文件路径
    prefix = '';                                         % 文件名前缀
    format = 'bmp';                                      %（图片）文件格式
    suffix = strcat('','.',format);                      % 文件后缀
%-------------------------------------------------------------------------------
Datalist = dir(strcat(pathway,'*.bin')); %dir函数读取的文件路径不是按照文件名排序的
files_name =natsortfiles({Datalist.name}); %用natsortfiles函数将文件名正常排序（natsortfiles非matlab自带的函数）
[n,~] = size(Datalist);
for k=1:n  %n   k=1:n
fid=fopen(strcat(pathway,files_name{k}),'rb');
OldRawData = fread(fid,'int16');
fclose ('all');
% fprintf('cell size is %d ',OldRawData(3));
OldRawData=OldRawData(33:end-32); %去掉头尾因为记录位产生的数据。2个32
%%%%%%%%%%%%%%%%FPGA Data recover to RawData%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%----------------------------data input--------------------------------------
Waveform=OldRawData;

PulPol = 1;             %"1" means positive pulses
if size(Waveform, 1) > 1  %caculate the number of 
Waveform = Waveform * PulPol;
else
Waveform = Waveform' * PulPol;
end
%------------------------------------------------------------------------------

%------------------这部分用于去除数据最开头非整个脉冲部分--------------------------
    Threshold_value=4000;        %这个值和FPGA判断脉冲的阈值Pulse_threhold一致(经典2100)
%      Threshold_value=Threshold_value*0.9;
    j=0;
    Data_Start_Position=zeros(10,1);
    for i=1:32:32*100      %length(Waveform)        
        if Waveform(i)<Threshold_value
          j=j+1;  
          Data_Start_Position(j)=i;
        end 
    end 
        j=0;
    Data_end_Position=zeros(20,1);
    for i=32:32:32*20      %length(Waveform)        
        if Waveform(length(Waveform)-i)<Threshold_value
          j=j+1;  
          Data_end_Position(j)=length(Waveform)-i;
        end 
    end 
    
    Waveform=Waveform(Data_Start_Position(1): Data_end_Position(1));
%------------------可以使用这部分计算周期---------------------------------------------    
%      Period=mode(nonzeros(diff(Data_Start_Position)));  %mode 求众数
%---------------计算周期失效时  需要手动输入周期------------------------------------------
%         Period =11*32;          %   一定要和脉冲采集的点数相等。Number of samples per period （change the Period!!!!!!!!）
%-----------------------------------------------------------------------------------

%-------------归一化在 0-2^14 -----------------------------------------
waveMax = max(Waveform); waveMin = min(Waveform);
Waveform = (Waveform - waveMin)*2^14/(waveMax - waveMin);
%--------------------------------------------------------------------
PulseNum =floor(length(Waveform) / Period);% Number of acquired pulses
new_Waveform_matir=zeros(floor(Period),PulseNum);
i=1;
for Column=1:PulseNum
new_Waveform_matir(:,Column)=Waveform(i:floor(Period)+i-1);
i=i+Period;
end
% tic;

%----------------观察脉冲截取情况---------------
% for z=1:100
%     plot(new_Waveform_matir(:,z));
%     axis tight;
%     pause(0.1);
% end
%----------------------------------------------------
source_noise_num=3;                              %当脉冲很窄时，可以调小这个值 （波形的噪声长度/32）
signal_pulse_len=10;                             %  初试脉冲有信号的点数（即波形突起的部分 这里输入要比实际小 够判断就行）     
Pulse_threhold_img_ratio=0.1;
Pulse_threhold_img=(max(Waveform)-min(Waveform)) * Pulse_threhold_img_ratio; %用于选择两个脉冲之间的噪声长度的阈值恢复紊乱可改这个值
startposition=find(Waveform>Pulse_threhold_img,1,'first');
Waveform_temp=Waveform(startposition:end);
noise_location=find(Waveform_temp<Pulse_threhold_img); 

noise_location_1=find(diff(noise_location)>signal_pulse_len);        
noise_length=diff([1;noise_location_1;length(noise_location)]);
add_noise_num=source_noise_num-round(noise_length/32);
% add_noise_num=add_noise_num(1:length(add_noise_num)-1);
%---------最初写法 太慢了------------------------------------------------------
% RawData=[];
% toc;
% for i=1:(size(new_Waveform_matir,2)-1)
%     RawData=[RawData;new_Waveform_matir(:,i)];
%     RawData=[RawData;zeros(add_noise_num(i)*32,1)];
% end
% toc;
%--------优化写法 很快----------------------------------------------------
RawData=zeros(length(new_Waveform_matir(:))+sum(add_noise_num(1:end-1))*32-Period,1);
add_noise_length=add_noise_num*32;
pulse_start_position=[0;cumsum(add_noise_length+Period)]+1;
for i=1:min( (size(new_Waveform_matir,2)-1),length(pulse_start_position) )
RawData(pulse_start_position(i):pulse_start_position(i)+Period-1)=new_Waveform_matir(:,i);
end
% toc;
%------------------------------------------------------------------
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%Utilize  code to recover RawData to image%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  

%-------------整体插值部分---------------------------------
RawData=InsertPoint(RawData,8);
%--------------------------------------------------------

if size(RawData, 1) > 1  %caculate the number of 
RawData = RawData*1 ;
else
RawData = RawData'*1;
end

RawData = RawData - mean(RawData);%将波形整体平移 正负对称
RawData_f = fft(RawData);
%------------------------绘制FFT频谱图----------------------------------------
FFTAmplitude = abs(RawData_f)/length(RawData(:));
%     plot(FFTAmplitude);                                         %绘制FFT频谱图
repeate=find(FFTAmplitude == max(FFTAmplitude), 1, 'first');%找出重复频率，与下面对比
%-----------------------------------------------------------------------------
%RepRate = find(RawData_f == max(RawData_f(1 : round(end / 2))), 1, 'first');%找出RawData_f最大值的位置（横坐标代表频率）
RepRate = find(RawData_f == max(RawData_f(1 : round(length(RawData_f)/ 2))), 1, 'first');
Width = round(0.3* length(RawData) / RepRate) * 2;

PassBand = round(1.5 * RepRate);
filter = [0; ones(PassBand, 1); zeros(length(RawData) - 1 - 2 * PassBand, 1); ones(PassBand, 1)];%前一个PassBand，后一个PassBand，把原始数据高频成分滤掉，剩下的就是比较规则的波形
FilterData = real(ifft(RawData_f .* filter));      %ifft 为离散快速傅里叶逆变换

SubData = FilterData(1 : Width * 3);               %这里Width系数旁边的原始系数为2.5，我改为了3. 原来2.5没有包含两个峰
[Peaks, Locs] = findpeaks(SubData);                %Peaks对应峰值，Locs对应峰位数
if Locs(1) - Width / 2 > 0
FirstPulsePos = Locs(1) - Width / 2;
else
FirstPulsePos = Locs(2) - Width / 2;
end
FirstPulse = RawData(FirstPulsePos : FirstPulsePos + Width);

CrossCor = [];
for i = length(RawData)- Width * 2.5 : length(RawData)- Width
CrossCor = [CrossCor, sum(FirstPulse .* RawData(i : i + Width))];
end
LastPulsePos = find(CrossCor == max(CrossCor), 1, 'last') + length(RawData)- Width * 2.5 - 1;

FilterData = FilterData(FirstPulsePos : LastPulsePos - 1);
ColNum = length(findpeaks(FilterData)) + 1;
Duration = (LastPulsePos - FirstPulsePos) / (ColNum - 1);
StartPoint = round((0 : ColNum - 1) * Duration) + FirstPulsePos - 1;%四舍五入取每一个脉冲开始的点
ImgIndex = reshape(repmat((0 : Width)', 1, ColNum) + repmat(StartPoint, Width + 1, 1), 1, []);%用repmat函数构造位置矩阵，再用reshape函数变为向量
if ImgIndex(1)==0     %会出现ImgIndex第一个数为0的情况，下面会报错，加这个判断防止报错。
   ImgIndex=ImgIndex+1;
end
Img = RawData(ImgIndex);   %按位置取  如果这行报错 则加大插值的数  或者修改Pulse_threhold_img的系数
Img = reshape(Img, Width + 1, []);

%-----------------剪切图像y部分----------------------------------------------------------------------------------
Img_first_column=Img(:,1);
Column_Start_position=find( Img_first_column>(min(Img_first_column)+(max(Img_first_column)-min(Img_first_column))*Pulse_threhold_img_ratio) );
if ~isempty(Column_Start_position)   %防止出BUG 不为空才剪切
 % Img=Img(Column_Start_position(1):min(Column_Start_position(1)+ColNum-40,Width ), : );
%---------------------用狭缝截取了脉冲用这个--------------------------------------------------------- 
Img=Img(Column_Start_position(1):min(Column_Start_position(end),Width), : );
%---------------------没有用狭缝截取，没有明显的跳变，用这个手动输入参数选--------------------------------------------------
% Img=Img(Column_Start_position(1):Column_Start_position(1)+round(size(Img,1)*(5.5/7.5)), : );%12.5*0.6=7.5  0.6是上面截取的
end
%--------------------------------------------------------------------------------------------------------------
figure(1);
% set(gca,'XTick',[]) % Remove the scale in the x axis!
% set(gca,'YTick',[]) % Remove the scale in the y axis
%   set(gcf,'Position',[500 500 200 200]); % [centerX， centerY，width， height]
% set(gca,'Position',[0 0 1 1]) % Make the axes occupy the hole figure
imagesc(Img);
% figure(2);
%  set(gca,'XTick',[]) % Remove the scale in the x axis!
%  set(gca,'YTick',[]) % Remove the scale in the y axis
%  set(gcf,'Position',[680 40 480 480]); % [centerX， centerY，width， height]
% set(gca,'Position',[0 0 1 1]) % Make the axes occupy the hole figure

if move_background == 1
    Background1 = mean(Img(:, 5 : 10), 2);
    Background2 = mean(Img(:, end - 10 : end-5), 2);
    if mean(Background1) >= mean(Background2)
        Background = Background1;
    else
        Background = Background2;
    end
    Img = Img - repmat(Background, 1, size(Img,2));
%-----------------剪切图像x部分(调整图像矩阵横纵比)---------------------------------
% Img=Img( 1:round(size(Img,1)) , 1:round(size(Img,2)*(40.45/45)) );%5.5ns=40.45um
%-------------------------------------------------------------------   
    figure(5);  
    imagesc(Img);  
    set(gca,'XTick',[]) % Remove the scale in the x axis!
    set(gca,'YTick',[]) % Remove the scale in the y axis
   %set(gcf,'Position',[500 500 200 200],'color','w'); % [centerX， centerY，width， height]
   set(gcf,'Position',[680 40 495 495],'color','w'); % [centerX， centerY，width， height]
    
%     pos = get(gcf,'Position');  %不用固定位置 只改变大小
%     pos(3:4) = [500 500]; 
%     set(gcf,'Position',pos); 
    
    set(gca,'Position',[0 0 1 1]) % Make the axes occupy the hole figure
    axis off;  
    colormap gray;
end
% figure(6);
% imagesc(Img);
% colormap gray;

% toc;
if plot_moving_avg == 1
    avg_length=32;
    pingjunzhi2=mean(Img);
    pingjunzhi_2 = smooth(pingjunzhi2,avg_length)';
    figure(3);
    set(gcf,'Position',[50 300 600 500]); % [centerX， centerY，width， height]
    plot(pingjunzhi2);
    axis tight;
    figure(4);
    set(gcf,'Position',[1275 300 600 500]); % [centerX， centerY，width， height]
    plot(pingjunzhi_2);
    axis tight;
end
 
%------------------------select the figure to save-----------------
%  saveas(figure(5),strcat(path, prefix, num2str(k-1)),'bmp');
 
OriginalFileName = regexp(files_name{k},'\.', 'split');   %save as OriginalFileName
saveas(figure(5),strcat(path,OriginalFileName{1}),'bmp');
%----------------------------------------------------------------------
 end
