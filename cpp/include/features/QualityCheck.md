维度	检查方向	核心检验内容	入库标准	技术要点
分布稳定性（Distribution Stability）	跨时间	不同时期特征分布形态（均值、方差、偏度、峰度）是否稳定	滚动窗口 KS-test / Wasserstein 距离 < 阈值（如 0.1）	使用时间分桶检验特征漂移；记录漂移路径
分布对称性（Distribution Symmetry）	横截面	检查偏度/峰度异常；识别单侧饱和或极值偏移	偏度 ∈ [-1,1]；峰度 < 10（或自定义）	对尾部值进行 Winsorize；异常分布打标
尺度鲁棒性（Scale Robustness）	正态化前后	特征分布在不同缩放方式下统计量稳定	不同 scale 下 Rank 相关 > 0.9	验证分布缩放不会改变排序结构
时序平稳性（Temporal Stationarity）	时间序列	检查单位根（ADF/P-P 检验）与滚动均值漂移	平稳性显著（p < 0.05）或弱平稳（差分后通过）	非平稳特征仅可入库为动态特征（标注）
时序一致性（Temporal Consistency）	不同频段	特征在不同时间粒度（1min/5min/1d）下统计特征一致	各尺度统计指标 RankCorr > 0.8	检验特征随采样频率变化的稳定性
自相关结构（Autocorrelation Structure）	时间序列	特征的自相关系数、PACF 是否快速衰减	lag>5 时自相关绝对值 < 0.2	高滞后自相关说明信息滞留或构造缺陷
异方差特征（Heteroscedasticity）	波动时段	在高低波动期，特征方差变化比率	方差比 < 3	若差异显著，标注 regime-dependent
信息半衰期（Information Half-life）	信号衰减	自协方差函数随滞后时间衰减至 0.5 的时间	半衰期 < 合理周期（如 5min/3d）	太长 → 滞后；太短 → 噪声
时间覆盖率（Temporal Coverage）	缺失分布	时间轴上缺失段比例与集中度	缺失率 < 5%，无连续断点	时间覆盖不足易导致样本偏差
结构突变（Structural Breaks）	长期稳定性	Chow test / CUSUM 检验结构突变	无显著突变（p>0.05）	检查数据源或构造逻辑变动
分布可分性（Separability）	分位切片	高低分位间统计显著差异（均值/方差）	Top vs Bottom 均值差显著（t>2）	验证特征具区分性但非预测性
噪声占比（Noise Ratio）	滚动窗口	方差分解：信号/噪声比	SNR > 1	噪声主导时，标记为高噪特征
时序相依性（Temporal Dependency）	与滞后自身	mutual information(lag) 峰值	信息峰值在小滞后范围内	识别时间耦合特征
非线性动态（Nonlinear Dynamics）	复杂性	计算近似熵/样本熵变化趋势	熵值稳定在合理区间	过高 = 噪声，过低 = 过度平滑
尾部鲁棒性（Tail Robustness）	极值事件	极端行情下的分布漂移比率	极端分位漂移 < 30%	评估在 crisis regime 下可用性
跨资产一致性（Cross-asset Consistency）	不同标的	特征在不同股票/期货上的统计形态一致性	KS-distance 平均 < 0.2	过度依赖单资产结构 = 不泛化
时间滞后敏感性（Lag Sensitivity）	构造延迟	滞后/提前一个采样周期后特征变化幅度	差分相关 < 0.2	检查延迟或未来信息污染
重采样稳定性（Resample Stability）	采样方案	改变 sampling 起点后统计一致	多起点间指标差异 < 5%	检验时间对齐依赖性
条件分布稳定性（Conditional Stability）	市况分层	在不同波动/成交量/流动性 regime 下分布差异	条件 KS < 0.15	确认特征不随市场状态剧烈漂移
