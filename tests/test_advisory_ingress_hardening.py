"""Malformed advisory evidence must be rejected before healthy/favorable output."""
import copy
import json
import math
import pytest
from core.finance_kernel.anomaly_detection import AnomalyDetectionOperator
from core.finance_kernel.chart_intelligence import ChartIntelligenceOperator
from core.finance_kernel.volume_advisory import IntradayVolumeAdvisory, calculate_hft_delta_v
from core.finance_kernel.wnfs import WNFSOperator

CASES = [
    (AnomalyDetectionOperator, {'last_price':100., 'ewma_volatility':.01, 'baseline_volatility':.01},
     ['last_price','ewma_volatility','baseline_volatility','rolling_correlation','volatility_threshold']),
    (ChartIntelligenceOperator, {'last_price':100., 'ewma_volatility':.01, 'baseline_volatility':.01},
     ['last_price','ewma_volatility','baseline_volatility','rolling_correlation','trust_score']),
    (IntradayVolumeAdvisory, {'current_volume':2000.,'avg_volume':1000.,'price_change':.01,'vwap_deviation':.001},
     ['current_volume','avg_volume','price_change','hft_mass','stabilizer_factor']),
    (WNFSOperator, {'sequence_id':1}, ['bid_size','ask_size','last_price','vwap_delta']),
]
BAD = [math.nan, math.inf, -math.inf, True, '0.1', None, 10**400]

@pytest.mark.parametrize('cls,base,fields', CASES)
@pytest.mark.parametrize('value', BAD)
@pytest.mark.parametrize('direct', [False, True])
def test_advisory_rejects_malformed_numeric_evidence(cls,base,fields,value,direct):
    for field in fields:
        op=cls()
        data=op.preprocess(base) if direct else dict(base)
        data[field]=value
        with pytest.raises(ValueError, match=field):
            op.execute(data) if direct else op.preprocess(data)
        if isinstance(op, ChartIntelligenceOperator):
            assert op._prev_modifier is None

@pytest.mark.parametrize('field', ['is_index_etf','enable_hft','enable_contrarian_oversold','stabilizer_risk_elevated'])
def test_volume_control_flags_require_booleans(field):
    with pytest.raises(ValueError, match=field):
        IntradayVolumeAdvisory().preprocess(dict(CASES[2][1], **{field:'false'}))

@pytest.mark.parametrize('field,value', [('sequence_id',-1),('sequence_id',True),('sequence_id',1.5),('sequence_id',2**64-1),('wave_channel_id',256),('frame_flags',8),('channel_status',4),('max_sequence_gaps',0)])
def test_wnfs_rejects_invalid_sequence_policy(field,value):
    with pytest.raises(ValueError, match=field):
        WNFSOperator().preprocess(dict(CASES[3][1], **{field:value}))

@pytest.mark.parametrize('field', ['vol_5m','liq_30d','vol_corr_1h'])
def test_prepared_chart_baseline_cannot_bypass_validation(field):
    op=ChartIntelligenceOperator()
    data=op.preprocess(CASES[1][1])
    setattr(data['baseline'], field, math.nan)
    with pytest.raises(ValueError, match=field):
        op.execute(data)
    assert op._prev_modifier is None

@pytest.mark.parametrize('cls,base,fields', CASES)
def test_valid_advisory_recovery_and_json_evidence(cls,base,fields):
    op=cls()
    original=copy.deepcopy(base)
    result=op.execute(op.preprocess(base))
    json.dumps(result, allow_nan=False)
    assert base == original

@pytest.mark.parametrize('field', ['isp','efficiency','alpha','v0','min_mass_floor'])
def test_hft_math_rejects_non_finite_parameters(field):
    data=dict(isp=1.,efficiency=.95,alpha=.1,v0=0.,min_mass_floor=1e-6,ticks=[{'p_input':.1,'w':.1,'v':2.,'M':1.,'dt':.001}])
    data[field]=math.nan
    with pytest.raises(ValueError, match=field):
        calculate_hft_delta_v(**data)
