import React, { useEffect, useState } from 'react';
import { fetchLiveTraffic } from '../services/api';

const Traffic = () => {
    const [sessions, setSessions] = useState([]);

    useEffect(() => {
        const loadData = async () => {
            try {
                const data = await fetchLiveTraffic();
                setSessions(data);
            } catch (err) { console.error("Failed to fetch traffic", err); }
        };

        loadData();
        const interval = setInterval(loadData, 3000); // רענון כל 3 שניות
        return () => clearInterval(interval);
    }, []);

    return (
        <div className="p-6">
            <h1 className="text-2xl font-bold mb-4 text-white">Live Network Traffic</h1>
            <div className="overflow-x-auto bg-gray-800 rounded-lg shadow">
                <table className="min-w-full text-left text-gray-300">
                    <thead className="bg-gray-700 text-gray-400 uppercase text-xs">
                        <tr>
                            <th className="px-6 py-3">Protocol</th>
                            <th className="px-6 py-3">Source</th>
                            <th className="px-6 py-3">Destination</th>
                            <th className="px-6 py-3">Last Seen</th>
                        </tr>
                    </thead>
                    <tbody className="divide-y divide-gray-700">
                        {sessions.map((s, idx) => (
                            <tr key={idx} className="hover:bg-gray-750 transition-colors">
                                <td className="px-6 py-4 font-mono text-blue-400">{s.protocol}</td>
                                <td className="px-6 py-4">{s.src_ip}:{s.src_port}</td>
                                <td className="px-6 py-4">{s.dst_ip}:{s.dst_port}</td>
                                <td className="px-6 py-4 text-sm text-gray-500">
                                    {new Date(s.last_seen).toLocaleTimeString()}
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>
        </div>
    );
};

export default Traffic;