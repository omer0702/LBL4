const API_BASE_URL = "http://localhost:8000/api";

export const fetchOverview = async () => {
    const response = await fetch(`${API_BASE_URL}/overview`);
    if (!response.ok) throw new Error("Failed to fetch overview");
    return response.json();
};

export const fetchEvents = async () => {
    const response = await fetch(`${API_BASE_URL}/events`);
    if (!response.ok) throw new Error("Failed to fetch events");
    return response.json();
};

export const checkSystemHealth = async () => {
    const response = await fetch(`${API_BASE_URL}/health`);
    return response.json();
};

export const fetchServiceMetrics = async (id) => {
    const response = await fetch(`${API_BASE_URL}/services/${id}/metrics`);
    return response.json();
};

export const fetchServiceBackends = async (id) => {
    const response = await fetch(`${API_BASE_URL}/services/${id}/backends`);
    return response.json();
};


export const adminDeployCluster = async () => {
    const res = await fetch(`${API_BASE_URL}/admin/deploy-cluster`, { method: 'POST' });
    return res.json();
};

export const adminCleanup = async () => {
    const res = await fetch(`${API_BASE_URL}/admin/cleanup`, { method: 'POST' });
    return res.json();
};

export const adminRunStress = async () => {
    const res = await fetch(`${API_BASE_URL}/admin/run-stress`, { method: 'POST' });
    return res.json();
};

export const adminAddBackend = async (suffix, id) => {
    const res = await fetch(`${API_BASE_URL}/admin/add-backend/${suffix}/${id}`, { method: 'POST' });
    return res.json();
};

export const fetchServicePerformance = async (id) => {
    const response = await fetch(`${API_BASE_URL}/services/${id}/performance`);
    return response.json();
}

export const fetchLiveTraffic = async () => {
    const response = await fetch(`${API_BASE_URL}/live-traffic`);
    return response.json();
};