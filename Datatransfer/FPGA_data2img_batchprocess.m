%----------------setting configure----------------------------------------------
PulPol = 1;             %"1" means positive pulses/"-1"means negtive pulses
plot_moving_avg=0;    %1=on 0=off
move_background=1;    %1=on 0=off
      
Period =32*3;              %�ڴ��ڰ�����������ʱ����Ҫ�ֶ���������-
                          % һ��Ҫ������ɼ��ĵ�����ȡ�Number of samples per period ��change the Period!!!!!!!!��
%-----------------------------------------------------------------------------------

%----------------���ûָ������ļ�·��---------------------------------------------
 pathway='sample\';
%-------------------------------------------------------------------------------

%------------------���ñ����ͼƬλ��·�� ��ʽ ��׺------------------------------
    path = 'Img\';  %�ļ�·��
    prefix = '';                                         % �ļ���ǰ׺
    format = 'bmp';                                      %��ͼƬ���ļ���ʽ
    suffix = strcat('','.',format);                      % �ļ���׺
%-------------------------------------------------------------------------------
Datalist = dir(strcat(pathway,'*.bin')); %dir������ȡ���ļ�·�����ǰ����ļ��������
files_name =natsortfiles({Datalist.name}); %��natsortfiles�������ļ�����������natsortfiles��matlab�Դ��ĺ�����
[n,~] = size(Datalist);
for k=1:n  %n   k=1:n
fid=fopen(strcat(pathway,files_name{k}),'rb');
OldRawData = fread(fid,'int16');
fclose ('all');
% fprintf('cell size is %d ',OldRawData(3));
OldRawData=OldRawData(33:end-32); %ȥ��ͷβ��Ϊ��¼λ���������ݡ�2��32
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

%------------------�ⲿ������ȥ�������ͷ���������岿��--------------------------
    Threshold_value=4000;        %���ֵ��FPGA�ж��������ֵPulse_threholdһ��(����2100)
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
%------------------����ʹ���ⲿ�ּ�������---------------------------------------------    
%      Period=mode(nonzeros(diff(Data_Start_Position)));  %mode ������
%---------------��������ʧЧʱ  ��Ҫ�ֶ���������------------------------------------------
%         Period =11*32;          %   һ��Ҫ������ɼ��ĵ�����ȡ�Number of samples per period ��change the Period!!!!!!!!��
%-----------------------------------------------------------------------------------

%-------------��һ���� 0-2^14 -----------------------------------------
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

%----------------�۲������ȡ���---------------
% for z=1:100
%     plot(new_Waveform_matir(:,z));
%     axis tight;
%     pause(0.1);
% end
%----------------------------------------------------
source_noise_num=3;                              %�������խʱ�����Ե�С���ֵ �����ε���������/32��
signal_pulse_len=10;                             %  �����������źŵĵ�����������ͻ��Ĳ��� ��������Ҫ��ʵ��С ���жϾ��У�     
Pulse_threhold_img_ratio=0.1;
Pulse_threhold_img=(max(Waveform)-min(Waveform)) * Pulse_threhold_img_ratio; %����ѡ����������֮����������ȵ���ֵ�ָ����ҿɸ����ֵ
startposition=find(Waveform>Pulse_threhold_img,1,'first');
Waveform_temp=Waveform(startposition:end);
noise_location=find(Waveform_temp<Pulse_threhold_img); 

noise_location_1=find(diff(noise_location)>signal_pulse_len);        
noise_length=diff([1;noise_location_1;length(noise_location)]);
add_noise_num=source_noise_num-round(noise_length/32);
% add_noise_num=add_noise_num(1:length(add_noise_num)-1);
%---------���д�� ̫����------------------------------------------------------
% RawData=[];
% toc;
% for i=1:(size(new_Waveform_matir,2)-1)
%     RawData=[RawData;new_Waveform_matir(:,i)];
%     RawData=[RawData;zeros(add_noise_num(i)*32,1)];
% end
% toc;
%--------�Ż�д�� �ܿ�----------------------------------------------------
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

%-------------�����ֵ����---------------------------------
RawData=InsertPoint(RawData,8);
%--------------------------------------------------------

if size(RawData, 1) > 1  %caculate the number of 
RawData = RawData*1 ;
else
RawData = RawData'*1;
end

RawData = RawData - mean(RawData);%����������ƽ�� �����Գ�
RawData_f = fft(RawData);
%------------------------����FFTƵ��ͼ----------------------------------------
FFTAmplitude = abs(RawData_f)/length(RawData(:));
%     plot(FFTAmplitude);                                         %����FFTƵ��ͼ
repeate=find(FFTAmplitude == max(FFTAmplitude), 1, 'first');%�ҳ��ظ�Ƶ�ʣ�������Ա�
%-----------------------------------------------------------------------------
%RepRate = find(RawData_f == max(RawData_f(1 : round(end / 2))), 1, 'first');%�ҳ�RawData_f���ֵ��λ�ã����������Ƶ�ʣ�
RepRate = find(RawData_f == max(RawData_f(1 : round(length(RawData_f)/ 2))), 1, 'first');
Width = round(0.3* length(RawData) / RepRate) * 2;

PassBand = round(1.5 * RepRate);
filter = [0; ones(PassBand, 1); zeros(length(RawData) - 1 - 2 * PassBand, 1); ones(PassBand, 1)];%ǰһ��PassBand����һ��PassBand����ԭʼ���ݸ�Ƶ�ɷ��˵���ʣ�µľ��ǱȽϹ���Ĳ���
FilterData = real(ifft(RawData_f .* filter));      %ifft Ϊ��ɢ���ٸ���Ҷ��任

SubData = FilterData(1 : Width * 3);               %����Widthϵ���Աߵ�ԭʼϵ��Ϊ2.5���Ҹ�Ϊ��3. ԭ��2.5û�а���������
[Peaks, Locs] = findpeaks(SubData);                %Peaks��Ӧ��ֵ��Locs��Ӧ��λ��
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
StartPoint = round((0 : ColNum - 1) * Duration) + FirstPulsePos - 1;%��������ȡÿһ�����忪ʼ�ĵ�
ImgIndex = reshape(repmat((0 : Width)', 1, ColNum) + repmat(StartPoint, Width + 1, 1), 1, []);%��repmat��������λ�þ�������reshape������Ϊ����
if ImgIndex(1)==0     %�����ImgIndex��һ����Ϊ0�����������ᱨ��������жϷ�ֹ����
   ImgIndex=ImgIndex+1;
end
Img = RawData(ImgIndex);   %��λ��ȡ  ������б��� ��Ӵ��ֵ����  �����޸�Pulse_threhold_img��ϵ��
Img = reshape(Img, Width + 1, []);

%-----------------����ͼ��y����----------------------------------------------------------------------------------
Img_first_column=Img(:,1);
Column_Start_position=find( Img_first_column>(min(Img_first_column)+(max(Img_first_column)-min(Img_first_column))*Pulse_threhold_img_ratio) );
if ~isempty(Column_Start_position)   %��ֹ��BUG ��Ϊ�ղż���
 % Img=Img(Column_Start_position(1):min(Column_Start_position(1)+ColNum-40,Width ), : );
%---------------------�������ȡ�����������--------------------------------------------------------- 
Img=Img(Column_Start_position(1):min(Column_Start_position(end),Width), : );
%---------------------û���������ȡ��û�����Ե����䣬������ֶ��������ѡ--------------------------------------------------
% Img=Img(Column_Start_position(1):Column_Start_position(1)+round(size(Img,1)*(5.5/7.5)), : );%12.5*0.6=7.5  0.6�������ȡ��
end
%--------------------------------------------------------------------------------------------------------------
figure(1);
% set(gca,'XTick',[]) % Remove the scale in the x axis!
% set(gca,'YTick',[]) % Remove the scale in the y axis
%   set(gcf,'Position',[500 500 200 200]); % [centerX�� centerY��width�� height]
% set(gca,'Position',[0 0 1 1]) % Make the axes occupy the hole figure
imagesc(Img);
% figure(2);
%  set(gca,'XTick',[]) % Remove the scale in the x axis!
%  set(gca,'YTick',[]) % Remove the scale in the y axis
%  set(gcf,'Position',[680 40 480 480]); % [centerX�� centerY��width�� height]
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
%-----------------����ͼ��x����(����ͼ�������ݱ�)---------------------------------
% Img=Img( 1:round(size(Img,1)) , 1:round(size(Img,2)*(40.45/45)) );%5.5ns=40.45um
%-------------------------------------------------------------------   
    figure(5);  
    imagesc(Img);  
    set(gca,'XTick',[]) % Remove the scale in the x axis!
    set(gca,'YTick',[]) % Remove the scale in the y axis
   %set(gcf,'Position',[500 500 200 200],'color','w'); % [centerX�� centerY��width�� height]
   set(gcf,'Position',[680 40 495 495],'color','w'); % [centerX�� centerY��width�� height]
    
%     pos = get(gcf,'Position');  %���ù̶�λ�� ֻ�ı��С
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
    set(gcf,'Position',[50 300 600 500]); % [centerX�� centerY��width�� height]
    plot(pingjunzhi2);
    axis tight;
    figure(4);
    set(gcf,'Position',[1275 300 600 500]); % [centerX�� centerY��width�� height]
    plot(pingjunzhi_2);
    axis tight;
end
 
%------------------------select the figure to save-----------------
%  saveas(figure(5),strcat(path, prefix, num2str(k-1)),'bmp');
 
OriginalFileName = regexp(files_name{k},'\.', 'split');   %save as OriginalFileName
saveas(figure(5),strcat(path,OriginalFileName{1}),'bmp');
%----------------------------------------------------------------------
 end
