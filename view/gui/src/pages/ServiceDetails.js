import React, { useState, useEffect } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { ArrowLeft, ArrowRight, Activity, Server, Search, BarChart3, PieChart as PieIcon } from 'lucide-react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,PieChart, Pie, Cell, Legend } from 'recharts';
import { fetchServiceMetrics, fetchServiceBackends, fetchOverview , fetchServicePerformance} from '../services/api';
import { useGlobal } from '../context/GlobalContext';

const COLORS = ['#38bdf8', '#818cf8', '#c084fc', '#fb7185', '#34d399'];



const ServiceDetails = () => {
    const { id } = useParams();
    const currentId = parseInt(id) || 1; // ברירת מחדל ל-1 אם אין ID
    const navigate = useNavigate();
    const { services } = useGlobal(); // רשימת השירותים מה-Context לצורך דפדוף ופילטר
    const [metrics, setMetrics] = useState([]);
    const [backends, setBackends] = useState([]);
    const [loading, setLoading] = useState(true);
    const [perf, setPerf] = useState({packet_loss: 0, avg_latency: 0})

    // console.log(metrics);
    // console.log(backends);

    const safeMetrics = metrics.map(m => {
        const obj = { ...m };

        backends.forEach(b => {
            const key = `server_${b.logical_id}`;
            if (!(key in obj)) {
                obj[key] = 0;
            }
        });

        return obj;
    });


    const loadData = async () => {
        try {
            const [m, b, p] = await Promise.all([fetchServiceMetrics(currentId), fetchServiceBackends(currentId), fetchServicePerformance(currentId)]);
            setMetrics(Array.isArray(m) ? m : []);
            setBackends(Array.isArray(b) ? b : []);
            setPerf(Array.isArray(p) ? p : []);
        } catch (e) {
            console.error("Error loading service details:", e);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => {
        setLoading(true);
        loadData();
        const int = setInterval(loadData, 5000);
        return () => clearInterval(int);
    }, [currentId]);

    useEffect(() => {
        console.log("metrics:", metrics);
    }, [metrics]);

    // מצב טעינה ראשוני כדי למנוע קריסה של Recharts
    if (loading && metrics.length === 0) {
        return (
            <div className="p-10 flex flex-col items-center justify-center min-h-screen text-white">
                <div className="w-12 h-12 border-4 border-lb-accent border-t-transparent rounded-full animate-spin mb-4"></div>
                <p className="text-xl font-medium">Loading metrics from Database...</p>
            </div>
        );
    }

    // פונקציות דפדוף
    const goToNext = () => {
        const currentIndex = services.findIndex(s => s.service_id === currentId);
        if (currentIndex < services.length - 1) {
            navigate(`/services/${services[currentIndex + 1].service_id}`);
        }
    };

    const goToPrev = () => {
        const currentIndex = services.findIndex(s => s.service_id === currentId);
        if (currentIndex > 0) {
            navigate(`/services/${services[currentIndex - 1].service_id}`);
        }
    };

    const pieData = backends.length > 0
        ? backends.map(b => ({ name: `Server ${b.logical_id}`, value: b.pps || 0 }))
        : [{ name: "No Data", value: 0 }];

    return (
        <div className="p-8">
            {/* Header with Filter & Navigation */}
            <div className="flex flex-col md:flex-row md:items-center justify-between gap-4 mb-8">
                <div className="flex items-center gap-4">
                    <h1 className="text-2xl font-bold">Service Detail View</h1>
                    <div className="flex bg-lb-sidebar border border-slate-700 rounded-lg overflow-hidden">
                        <button onClick={goToPrev} className="p-2 hover:bg-slate-700 border-r border-slate-700"><ArrowLeft size={18}/></button>
                        <select 
                            value={currentId} 
                            onChange={(e) => navigate(`/services/${e.target.value}`)}
                            className="bg-transparent px-4 py-2 text-sm outline-none cursor-pointer"
                        >
                            {services.map(s => <option key={s.service_id} value={s.service_id}>{s.name} (ID: {s.service_id})</option>)}
                        </select>
                        <button onClick={goToNext} className="p-2 hover:bg-slate-700 border-l border-slate-700"><ArrowRight size={18}/></button>
                    </div>
                </div>
                {/*packet loss and latency */}
                <div className="flex items-center gap-4">
                    <div className="bg-lb-sidebar p-3 rounded-lg border border-slate-700">
                        <p className="text-sm text-slate-500">Packet Loss</p>
                        <p className={`font-bold ${perf.packet_loss > 0 ? 'text-red-400':'text-green-400'}`}>
                            {perf.packet_loss}%
                        </p>
                    </div>
                    <div className="bg-lb-sidebar p-3 rounded-lg border border-slate-700">
                        <p className="text-xs text-gray-500">AVG Latency</p>
                        <p className="font-bold text-lb-accent">{perf.avg_latency} ms</p>
                    </div>
                </div>
            </div>

            {/* Charts Grid */}
            <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-8">
                {/* Throughput Multi-Line Graph */}
                <div className="lg:col-span-2 bg-lb-sidebar p-6 rounded-xl border border-slate-700">
                    <h3 className="text-lg font-semibold mb-6 flex items-center gap-2">
                        <Activity size={20} className="text-lb-accent" /> Per-Backend Throughput (PPS)
                    </h3>
                    <div className="h-80">
                        <ResponsiveContainer width="100%" height="100%">
                            <LineChart data={safeMetrics}>
                                <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                                <XAxis dataKey="timestamp" hide />
                                <YAxis stroke="#64748b" />
                                <Tooltip contentStyle={{backgroundColor: '#1e293b', border: 'none'}} />
                                <Legend />
                                {/* קו לכל שרת שקיים ב-Data */}
                                {backends.map((b, i) => (
                                    <Line 
                                        key={b.logical_id}
                                        type="monotone" 
                                        dataKey={`server_${b.logical_id}`} 
                                        name={`Server ${b.logical_id}`}
                                        stroke={COLORS[i % COLORS.length]} 
                                        strokeWidth={2} 
                                        dot={false}
                                        isAnimationActive={false}
                                    />
                                ))}
                                {/* קו בולט לסך הכל */}
                                <Line 
                                    type="monotone" 
                                    dataKey="total" 
                                    name="Total Service PPS" 
                                    stroke="#ffffff" 
                                    strokeDasharray="5 5" 
                                    strokeWidth={1} 
                                    dot={false} 
                                />
                            </LineChart>
                        </ResponsiveContainer>
                    </div>
                </div>

                {/* Distribution Pie Chart */}
                <div className="lg:col-span-1 bg-lb-sidebar p-6 rounded-xl border border-slate-700 shadow-lg">
                    <h3 className="text-lg font-semibold mb-6 flex items-center gap-2 text-white">
                        <PieIcon size={20} className="text-purple-400" /> Traffic Distribution
                    </h3>
                    <div className="h-64">
                        <ResponsiveContainer width="100%" height="100%">
                            <PieChart>
                                <Pie
                                    data={pieData}
                                    innerRadius={60}
                                    outerRadius={80}
                                    paddingAngle={5}
                                    dataKey="value"
                                    isAnimationActive={false}
                                >
                                    {pieData.map((entry, index) => (
                                        <Cell key={`cell-${index}`} fill={COLORS[index % COLORS.length]} stroke="none" />
                                    ))}
                                </Pie>
                                <Tooltip
                                    contentStyle={{backgroundColor: '#1e293b', border: '1px solid #334155', borderRadius: '8px'}}
                                />
                            </PieChart>
                        </ResponsiveContainer>
                    </div>
                </div>
            </div>

            {/* Backend Table */}
            <div className="bg-lb-sidebar rounded-xl border border-slate-700 overflow-hidden shadow-lg">
                <div className="p-6 border-b border-slate-800 bg-slate-800/30">
                    <h3 className="text-lg font-semibold flex items-center gap-2 text-white">
                        <Server size={20} className="text-green-400" /> Backend Servers List
                    </h3>
                </div>
                <div className="overflow-x-auto">
                    <table className="w-full text-left">
                        <thead className="bg-slate-800/50 text-gray-400 text-xs uppercase tracking-wider">
                            <tr>
                                <th className="px-6 py-4">ID</th>
                                <th className="px-6 py-4">RIP Address</th>
                                <th className="px-6 py-4">Load (CPU/MEM)</th>
                                <th className="px-6 py-4">PPS</th>
                                <th className="px-6 py-4">Health Score</th>
                                <th className="px-6 py-4">Status</th>
                            </tr>
                        </thead>
                        <tbody className="divide-y divide-slate-800">
                            {backends.length > 0 ? (
                                backends.map(b => (
                                    <tr key={b.id} className="hover:bg-slate-800/50 transition-colors">
                                        <td className="px-6 py-4 font-mono text-sm text-gray-300 font-bold">#{b.logical_id}</td>
                                        <td className="px-6 py-4 text-blue-100 font-medium">{(b.ip).split('.').reverse().join('.')}:{b.port}</td>
                                        <td className="px-6 py-4">
                                            <div className="flex items-center gap-2">
                                                <div className="w-20 h-2 bg-slate-700 rounded-full overflow-hidden">
                                                    <div
                                                        className="h-full bg-lb-accent transition-all duration-500"
                                                        style={{width: `${Math.max(b.cpu, 5)}%`}}
                                                    ></div>
                                                </div>
                                                <span className="text-xs text-gray-400 font-mono">{b.cpu}% / {b.mem}%</span>
                                            </div>
                                        </td>
                                        <td className="px-6 py-4 text-sm font-semibold">{b.pps.toLocaleString()}</td>
                                        <td className="px-6 py-4">
                                            <span className={`font-bold ${
                                                b.score > 80 ? 'text-green-400' : b.score > 50 ? 'text-yellow-400' : 'text-red-400'
                                            }`}>
                                                {Math.round(b.score)}%
                                            </span>
                                        </td>
                                        <td className="px-6 py-4">
                                            <div className="flex items-center gap-2">
                                                <span className={`w-3 h-3 rounded-full ${b.active ? 'bg-green-500 animate-pulse' : 'bg-red-500'}`}></span>
                                                <span className="text-[10px] font-bold uppercase">{b.active ? 'Online' : 'Offline'}</span>
                                            </div>
                                        </td>
                                    </tr>
                                ))
                            ) : (
                                <tr>
                                    <td colSpan="6" className="px-6 py-10 text-center text-gray-500 italic">No backend servers associated with this service.</td>
                                </tr>
                            )}
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    );
};

export default ServiceDetails;