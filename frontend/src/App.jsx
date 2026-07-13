import React, { useState, useEffect, useRef } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';

const MAX_H = 100;

export default function App() {
  const [data,    setData]    = useState(null);
  const [status,  setStatus]  = useState('disconnected');
  const [history, setHistory] = useState([]);
  const [fps,     setFps]     = useState(0);
  const fpsRef = useRef(0);
  const wsRef  = useRef(null);

  // FPS counter
  useEffect(() => {
    const t = setInterval(() => { setFps(fpsRef.current); fpsRef.current = 0; }, 1000);
    return () => clearInterval(t);
  }, []);

  // WebSocket
  useEffect(() => {
    let ws, retryTimer;
    // Use the deployed backend URL from .env.production if set,
    // otherwise fall back to localhost for local development.
    const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const url   = process.env.REACT_APP_WS_URL || `${proto}//localhost:8000/ws/dashboard`;

    function connect() {
      setStatus('connecting');
      ws = new WebSocket(url);
      wsRef.current = ws;

      ws.onopen  = () => setStatus('connected');
      ws.onclose = () => { setStatus('disconnected'); retryTimer = setTimeout(connect, 2000); };
      ws.onerror = () => ws.close();

      ws.onmessage = ({ data: raw }) => {
        try {
          const d = JSON.parse(raw);
          if (d.heartbeat || d.ping) return;
          fpsRef.current++;
          setData(d);
          const t = new Date().toLocaleTimeString('en-GB', { hour12: false }).slice(3);
          setHistory(h => {
            const n = [...h, { t, kg: d.totalWeight }];
            return n.length > MAX_H ? n.slice(-MAX_H) : n;
          });
        } catch {}
      };
    }
    connect();
    return () => { clearTimeout(retryTimer); ws?.close(); };
  }, []);

  const kg  = data?.totalWeight ?? 0;
  const bx  = data?.balanceX   ?? 0.5;
  const by  = data?.balanceY   ?? 0.5;
  const lp  = data?.leftPercent  ?? 50;
  const rp  = data?.rightPercent ?? 50;
  const fp  = data?.frontPercent ?? 50;
  const bp  = data?.rearPercent  ?? 50;

  const statusColor = { connected: '#22c55e', connecting: '#f59e0b', disconnected: '#ef4444' };
  const statusLabel = { connected: 'LIVE', connecting: 'Connecting…', disconnected: 'Disconnected' };

  return (
    <div style={{ minHeight:'100vh', background:'#0a0f1e', color:'#f1f5f9',
                  fontFamily:'ui-monospace,monospace', padding:'16px' }}>

      {/* Header */}
      <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center',
                    borderBottom:'1px solid #1e293b', paddingBottom:'12px', marginBottom:'20px' }}>
        <div>
          <div style={{ fontSize:'16px', fontWeight:'bold', color:'#f8fafc' }}>
            ⚖️ Smart Balance Analyzer
          </div>
          <div style={{ fontSize:'11px', color:'#64748b', marginTop:'2px' }}>
            Topic: balance/sba20  ·  Single Load Cell
          </div>
        </div>
        <div style={{ display:'flex', alignItems:'center', gap:'8px' }}>
          {status === 'connected' && (
            <span style={{ fontSize:'11px', color:'#64748b' }}>{fps} Hz</span>
          )}
          <div style={{ width:'8px', height:'8px', borderRadius:'50%',
                        background: statusColor[status],
                        boxShadow: status==='connected' ? '0 0 8px #22c55e' : 'none' }} />
          <span style={{ fontSize:'12px', color: statusColor[status] }}>
            {statusLabel[status]}
          </span>
        </div>
      </div>

      {status !== 'connected' && (
        <div style={{ background:'#1c1007', border:'1px solid #854f0b', borderRadius:'8px',
                      padding:'10px 14px', fontSize:'12px', color:'#f59e0b', marginBottom:'16px' }}>
          Waiting for data…
        </div>
      )}

      {/* Main grid */}
      <div style={{ display:'grid', gridTemplateColumns:'1fr 1fr 1fr', gap:'12px', marginBottom:'12px' }}>

        {/* Total weight */}
        <div style={{ background:'#0f172a', border:'1px solid #1e293b', borderRadius:'12px',
                      padding:'20px', borderLeft:'3px solid #f59e0b' }}>
          <div style={{ fontSize:'10px', color:'#64748b', textTransform:'uppercase',
                        letterSpacing:'.1em', marginBottom:'8px' }}>Total Weight</div>
          <div style={{ fontSize:'36px', fontWeight:'bold', color:'#f8fafc' }}>
            {kg.toFixed(2)}
          </div>
          <div style={{ fontSize:'13px', color:'#94a3b8' }}>kg</div>
        </div>

        {/* Load cell 1 */}
        <div style={{ background:'#0f172a', border:'1px solid #1e293b', borderRadius:'12px',
                      padding:'20px', borderLeft:'3px solid #3b82f6' }}>
          <div style={{ fontSize:'10px', color:'#64748b', textTransform:'uppercase',
                        letterSpacing:'.1em', marginBottom:'8px' }}>LC1 — Load Cell</div>
          <div style={{ fontSize:'36px', fontWeight:'bold', color:'#f8fafc' }}>
            {(data?.load1 ?? 0).toFixed(2)}
          </div>
          <div style={{ fontSize:'13px', color:'#94a3b8' }}>kg</div>
        </div>

        {/* Balance platform */}
        <div style={{ background:'#0f172a', border:'1px solid #1e293b', borderRadius:'12px',
                      padding:'20px' }}>
          <div style={{ fontSize:'10px', color:'#64748b', textTransform:'uppercase',
                        letterSpacing:'.1em', marginBottom:'8px' }}>Balance Point</div>
          <div style={{ position:'relative', width:'100%', paddingBottom:'100%' }}>
            <div style={{ position:'absolute', inset:0, border:'1px solid #334155',
                          borderRadius:'6px', background:'#020617' }}>
              {/* crosshair */}
              <div style={{ position:'absolute', top:'50%', left:0, right:0,
                            height:'1px', background:'#1e293b' }} />
              <div style={{ position:'absolute', left:'50%', top:0, bottom:0,
                            width:'1px', background:'#1e293b' }} />
              {/* dot */}
              <div style={{
                position:'absolute',
                left: `${bx * 100}%`, top: `${by * 100}%`,
                transform:'translate(-50%,-50%)',
                width:'14px', height:'14px', borderRadius:'50%',
                background:'radial-gradient(circle at 35% 35%, #fde047, #f59e0b)',
                boxShadow:'0 0 10px rgba(250,204,21,0.8)',
                transition:'left 150ms ease-out, top 150ms ease-out'
              }} />
            </div>
          </div>
          <div style={{ fontSize:'10px', color:'#475569', marginTop:'6px', textAlign:'center' }}>
            X:{bx.toFixed(3)} Y:{by.toFixed(3)}
          </div>
        </div>
      </div>

      {/* Distribution */}
      <div style={{ background:'#0f172a', border:'1px solid #1e293b', borderRadius:'12px',
                    padding:'16px', marginBottom:'12px' }}>
        <div style={{ fontSize:'10px', color:'#64748b', textTransform:'uppercase',
                      letterSpacing:'.1em', marginBottom:'12px' }}>Weight Distribution</div>
        <div style={{ display:'grid', gridTemplateColumns:'1fr 1fr', gap:'16px' }}>
          {[
            { label:'Left / Right', a:'Left', b:'Right', va:lp, vb:rp, ca:'#3b82f6', cb:'#8b5cf6' },
            { label:'Front / Rear', a:'Front', b:'Rear', va:fp, vb:bp, ca:'#f59e0b', cb:'#10b981' },
          ].map(({ label, a, b, va, vb, ca, cb }) => (
            <div key={label}>
              <div style={{ display:'flex', justifyContent:'space-between', fontSize:'11px',
                            color:'#94a3b8', marginBottom:'6px' }}>
                <span style={{ color: ca }}>{a} {va.toFixed(1)}%</span>
                <span style={{ color: cb }}>{b} {vb.toFixed(1)}%</span>
              </div>
              <div style={{ display:'flex', height:'8px', borderRadius:'4px',
                            overflow:'hidden', background:'#0f172a' }}>
                <div style={{ width:`${va}%`, background:ca, transition:'width 200ms' }} />
                <div style={{ width:`${vb}%`, background:cb, transition:'width 200ms' }} />
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Chart */}
      <div style={{ background:'#0f172a', border:'1px solid #1e293b', borderRadius:'12px',
                    padding:'16px' }}>
        <div style={{ fontSize:'10px', color:'#64748b', textTransform:'uppercase',
                      letterSpacing:'.1em', marginBottom:'12px' }}>
          Weight History ({history.length}/100 samples)
        </div>
        <div style={{ height:'160px' }}>
          <ResponsiveContainer width="100%" height="100%">
            <LineChart data={history}>
              <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
              <XAxis dataKey="t" tick={{ fill:'#475569', fontSize:9 }}
                     axisLine={{ stroke:'#1e293b' }} tickLine={false}
                     interval="preserveStartEnd" />
              <YAxis tick={{ fill:'#475569', fontSize:9 }} axisLine={{ stroke:'#1e293b' }}
                     tickLine={false} unit=" kg" width={50} />
              <Tooltip contentStyle={{ background:'#0f172a', border:'1px solid #334155',
                                       fontSize:'11px', fontFamily:'monospace' }}
                       labelStyle={{ color:'#94a3b8' }} />
              <Line type="monotone" dataKey="kg" name="Weight" stroke="#f59e0b"
                    strokeWidth={2} dot={false} isAnimationActive={false} />
            </LineChart>
          </ResponsiveContainer>
        </div>
      </div>

    </div>
  );
}
